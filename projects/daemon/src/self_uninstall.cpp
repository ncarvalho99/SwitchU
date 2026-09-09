#include "self_uninstall.hpp"

#include <switchu/file_log.hpp>
#include <switchu/sd_commit.hpp>
#include <switchu/self_uninstall.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace switchu::daemon::self_uninstall {
namespace {

bool inspectRegularFile(const char* path, bool& exists) {
    struct stat info {};
    // lstat deliberately rejects a symlink even when its target is regular;
    // self-removal is permitted to touch only the exact physical override file.
    if (lstat(path, &info) != 0) {
        exists = false;
        return errno == ENOENT;
    }
    exists = true;
    return S_ISREG(info.st_mode);
}

bool isRegularFile(const char* path) {
    bool exists = false;
    return inspectRegularFile(path, exists) && exists;
}

bool requestIsValid() {
    using namespace switchu::self_uninstall;

    std::FILE* request = std::fopen(kRequest, "rb");
    if (!request)
        return false;
    char contents[sizeof(kRequestContents)]{};
    const size_t expectedSize = sizeof(kRequestContents) - 1;
    const size_t read = std::fread(contents, 1, expectedSize, request);
    const bool atEnd = std::fgetc(request) == EOF && std::ferror(request) == 0;
    std::fclose(request);
    return read == expectedSize && atEnd
        && std::memcmp(contents, kRequestContents, expectedSize) == 0;
}

} // namespace

StagedRequestResult applyStagedRequest() {
    using namespace switchu::self_uninstall;

    bool requestExists = false;
    const bool requestRegular = inspectRegularFile(kRequest, requestExists);
    if (!requestExists && requestRegular)
        return StagedRequestResult::None;
    if (!requestRegular) {
        switchu::FileLog::log("[uninstall] invalid request path retained");
        return StagedRequestResult::Pending;
    }
    if (!requestIsValid()) {
        switchu::FileLog::log("[uninstall] malformed request retained");
        return StagedRequestResult::Pending;
    }

    bool activeExists = false;
    bool disabledExists = false;
    const bool activeRegular = inspectRegularFile(kActiveOverride, activeExists);
    const bool disabledRegular = inspectRegularFile(kDisabledOverride, disabledExists);
    if (!activeRegular || !disabledRegular || !activeExists || disabledExists) {
        switchu::FileLog::log(
            "[uninstall] request retained: active_exists=%d active_regular=%d "
            "disabled_exists=%d disabled_regular=%d; manual recovery required",
            activeExists ? 1 : 0, activeRegular ? 1 : 0,
            disabledExists ? 1 : 0, disabledRegular ? 1 : 0);
        return StagedRequestResult::Pending;
    }

    if (std::rename(kActiveOverride, kDisabledOverride) != 0) {
        switchu::FileLog::log("[uninstall] disable override rename FAIL");
        return StagedRequestResult::Pending;
    }

    if (isRegularFile(kActiveOverride) || !isRegularFile(kDisabledOverride)) {
        switchu::FileLog::log("[uninstall] post-rename verification FAIL; attempting rollback");
        (void)std::rename(kDisabledOverride, kActiveOverride);
        (void)switchu::commitSdCard("uninstall rollback");
        return StagedRequestResult::Pending;
    }

    if (!switchu::commitSdCard("disable SwitchU override")) {
        switchu::FileLog::log("[uninstall] commit FAIL; attempting rollback");
        (void)std::rename(kDisabledOverride, kActiveOverride);
        (void)switchu::commitSdCard("uninstall rollback");
        return StagedRequestResult::Pending;
    }

    if (std::remove(kRequest) != 0) {
        // The override is already disabled. Retaining a stale marker is safe:
        // the next boot recognizes the preserved .disabled recovery file.
        switchu::FileLog::log("[uninstall] request cleanup FAIL; override remains disabled");
        return StagedRequestResult::Applied;
    }

    if (!switchu::commitSdCard("uninstall request cleanup")) {
        // The state required to return HOME has already been committed. Do not
        // roll it back merely because deleting an optional marker could not be
        // committed; the preserved recovery file makes a retry harmless.
        switchu::FileLog::log("[uninstall] cleanup commit FAIL; override remains disabled");
        return StagedRequestResult::Applied;
    }
    switchu::FileLog::log("[uninstall] override disabled; stock qlaunch starts now");
    return StagedRequestResult::Applied;
}

} // namespace switchu::daemon::self_uninstall
