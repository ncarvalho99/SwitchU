#include "update_apply.hpp"

#include "themeshop/ZipReader.hpp"
#include <switchu/file_log.hpp>

#include <cstdio>
#include <string>
#include <sys/stat.h>

// Applying an update belongs here, not in the menu.
//
// The menu cannot replace the files it is running from: SDL_ttf holds the font
// open for as long as the menu lives, and the first attempt to update in place
// destroyed that font and left the installation half replaced. The daemon runs
// before the menu is launched, so at this moment nothing in switch/SwitchU is
// open by anybody. The menu only downloads and verifies; deciding that the
// payload is good and putting it in place are separate jobs on purpose.
namespace switchu::daemon::update {
namespace {

constexpr const char* kStagedArchive = "sdmc:/config/SwitchU/update/update.zip";
// Written last by the menu, once the archive is complete and verified. Its
// presence is the whole signal: a partial download leaves no marker and is
// simply ignored here.
constexpr const char* kReadyMarker   = "sdmc:/config/SwitchU/update/ready";
constexpr const char* kAttemptFile   = "sdmc:/config/SwitchU/update/attempts";
constexpr const char* kCardRoot      = "sdmc:/";
// An update that cannot be applied must not delay every boot forever. After
// this many tries the staging is cleared and the console starts normally.
constexpr int kMaxAttempts = 3;

bool exists(const char* path) {
    struct stat info {};
    return stat(path, &info) == 0;
}

int readAttempts() {
    std::FILE* f = std::fopen(kAttemptFile, "rb");
    if (!f)
        return 0;
    int value = 0;
    if (std::fscanf(f, "%d", &value) != 1)
        value = 0;
    std::fclose(f);
    return value;
}

void writeAttempts(int value) {
    std::FILE* f = std::fopen(kAttemptFile, "wb");
    if (!f)
        return;
    std::fprintf(f, "%d", value);
    std::fclose(f);
}

void clearStaging() {
    std::remove(kReadyMarker);
    std::remove(kAttemptFile);
    std::remove(kStagedArchive);
}

} // namespace

void applyStagedUpdate() {
    if (!exists(kReadyMarker))
        return;
    if (!exists(kStagedArchive)) {
        switchu::FileLog::log("[update] marker without an archive; clearing");
        clearStaging();
        return;
    }

    const int attempts = readAttempts() + 1;
    if (attempts > kMaxAttempts) {
        switchu::FileLog::log("[update] giving up after %d attempts; booting as installed",
                              kMaxAttempts);
        clearStaging();
        return;
    }
    // Recorded before the work starts, so a failure that never returns still
    // counts against the limit and cannot loop the console forever.
    writeAttempts(attempts);

    switchu::FileLog::log("[update] applying staged update (attempt %d)", attempts);

    themeshop::ZipExtractPolicy policy;
    policy.allowExecutablePayload = true;
    policy.requiredRoots = {"atmosphere/", "switch/"};

    const auto result = themeshop::extractZipFile(kStagedArchive, kCardRoot, {}, policy);
    if (!result.success) {
        switchu::FileLog::log("[update] apply failed: %s", result.error.c_str());
        return;   // staging stays; the next boot retries until the limit
    }

    switchu::FileLog::log("[update] applied %d files, %llu bytes",
                          result.filesWritten, (unsigned long long)result.bytesWritten);
    clearStaging();
}

} // namespace switchu::daemon::update
