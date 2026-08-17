#include "GameModManager.hpp"

#include <fmt/format.h>
#include <switchu/fs_remove.hpp>

#include <algorithm>
#include <system_error>

namespace mods {
namespace {

std::filesystem::path titleRoot(std::uint64_t titleId) {
    return std::filesystem::path("sdmc:/atmosphere/contents") / fmt::format("{:016X}", titleId);
}

bool isWithin(const std::filesystem::path& child, const std::filesystem::path& parent) {
    const auto normalizedChild = child.lexically_normal();
    const auto normalizedParent = parent.lexically_normal();
    auto c = normalizedChild.begin();
    const auto ce = normalizedChild.end();
    auto p = normalizedParent.begin();
    const auto pe = normalizedParent.end();
    for (; p != pe; ++p, ++c) {
        if (c == ce || *c != *p) return false;
    }
    return true;
}

void scanDirectory(std::vector<Entry>& out, const std::filesystem::path& path,
                   Kind kind, bool enabled) {
    std::error_code ec;
    if (!std::filesystem::is_directory(path, ec) || ec) return;
    for (const auto& item : std::filesystem::directory_iterator(path, ec)) {
        if (ec) break;
        const auto name = item.path().filename().string();
        if (name.empty() || name[0] == '.') continue;
        out.push_back({name, kind, enabled, item.path()});
    }
}

Result fail(const std::error_code& ec) {
    return {false, ec ? ec.message() : "invalid mod path"};
}

} // namespace

const char* GameModManager::kindDirectory(Kind kind) {
    switch (kind) {
        case Kind::Romfs: return "romfs";
        case Kind::Exefs: return "exefs";
        case Kind::Cheats: return "cheats";
    }
    return "romfs";
}

std::vector<Entry> GameModManager::scan(std::uint64_t titleId) {
    std::vector<Entry> entries;
    const auto root = titleRoot(titleId);
    for (const auto kind : {Kind::Romfs, Kind::Exefs, Kind::Cheats}) {
        const auto directory = kindDirectory(kind);
        scanDirectory(entries, root / directory, kind, true);
        scanDirectory(entries, root / ".switchu-disabled" / directory, kind, false);
    }
    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        if (a.enabled != b.enabled) return a.enabled > b.enabled;
        if (a.kind != b.kind) return (int)a.kind < (int)b.kind;
        return a.name < b.name;
    });
    return entries;
}

Result GameModManager::toggle(const Entry& entry) {
    std::error_code ec;
    if (entry.name.empty() || !std::filesystem::exists(entry.path, ec) || ec)
        return fail(ec);

    // Compute the title root from the canonical layout, rather than accepting
    // a caller-provided destination. This prevents a malformed entry from
    // ever moving content outside atmosphere/contents/<titleId>.
    auto path = entry.path.lexically_normal();
    std::filesystem::path root;
    for (auto it = path.begin(); it != path.end(); ++it) {
        root /= *it;
        if (*it == "contents") {
            ++it;
            if (it == path.end()) return {false, "missing title directory"};
            root /= *it;
            break;
        }
    }
    const auto kind = std::filesystem::path(kindDirectory(entry.kind));
    const auto active = root / kind / entry.name;
    const auto disabled = root / ".switchu-disabled" / kind / entry.name;
    const auto from = entry.enabled ? active : disabled;
    const auto to = entry.enabled ? disabled : active;
    if (!isWithin(from, root) || !isWithin(to, root) || from != path)
        return {false, "unsafe mod path"};
    if (std::filesystem::exists(to, ec) && !ec)
        return {false, "a mod with this name already exists"};
    ec.clear();
    std::filesystem::create_directories(to.parent_path(), ec);
    if (ec) return fail(ec);
    std::filesystem::rename(from, to, ec);
    return ec ? fail(ec) : Result{true, {}};
}

Result GameModManager::remove(const Entry& entry) {
    std::error_code ec;
    if (entry.name.empty() || !std::filesystem::exists(entry.path, ec) || ec)
        return fail(ec);
    auto path = entry.path.lexically_normal();
    std::filesystem::path root;
    for (auto it = path.begin(); it != path.end(); ++it) {
        root /= *it;
        if (*it == "contents") {
            ++it;
            if (it == path.end()) return {false, "missing title directory"};
            root /= *it;
            break;
        }
    }
    if (!isWithin(path, root)) return {false, "unsafe mod path"};
    // Same POSIX walk the themes use. std::filesystem::remove_all does not work
    // against fsdev paths here -- it left theme folders on the card while
    // reporting through an error code nobody read, and this was the twin of
    // that call.
    std::string failedPath;
    if (!switchu::removeRecursive(path.string(), &failedPath))
        return {false, failedPath.empty() ? std::string("could not remove the mod")
                                          : ("could not remove " + failedPath)};
    return Result{true, {}};
}

} // namespace mods
