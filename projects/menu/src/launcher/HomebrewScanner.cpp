#include "HomebrewScanner.hpp"
#include "core/DebugLog.hpp"

#include <switch.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace homebrew {
namespace {

// The asset section follows the code, at the offset the NRO header calls its
// own size. libnx names every field involved, so none of this is guesswork.
constexpr std::size_t kNroStartSize = sizeof(NroStart);

bool readAt(std::FILE* f, long offset, void* dst, std::size_t size) {
    if (std::fseek(f, offset, SEEK_SET) != 0)
        return false;
    return std::fread(dst, 1, size, f) == size;
}

// A NACP holds sixteen language entries and the console's language is not known
// here, so the first non-empty one is used. hbmenu does the same, which keeps
// the two lists reading alike.
bool firstNamedLanguage(const NacpStruct& nacp, std::string& name, std::string& author) {
    for (const auto& lang : nacp.lang) {
        if (lang.name[0] == '\0')
            continue;
        name.assign(lang.name, strnlen(lang.name, sizeof(lang.name)));
        author.assign(lang.author, strnlen(lang.author, sizeof(lang.author)));
        return true;
    }
    return false;
}

bool readEntry(const std::string& path, Entry& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        DebugLog::log("[homebrew] open failed: %s", path.c_str());
        return false;
    }

    struct Closer {
        std::FILE* f;
        ~Closer() { if (f) std::fclose(f); }
    } closer{f};

    NroHeader header{};
    if (!readAt(f, (long)kNroStartSize, &header, sizeof(header)))
        return false;
    if (header.magic != NROHEADER_MAGIC) {
        DebugLog::log("[homebrew] not an NRO (magic=0x%08X): %s",
                      (unsigned)header.magic, path.c_str());
        return false;
    }

    out.path = path;
    // The filename without its extension, so an NRO with no asset section still
    // gets a readable label rather than being dropped.
    out.name = std::filesystem::path(path).stem().string();

    NroAssetHeader asset{};
    const long assetOffset = (long)header.size;
    if (!readAt(f, assetOffset, &asset, sizeof(asset)))
        return true;
    if (asset.magic != NROASSETHEADER_MAGIC) {
        // Legal: an NRO built without assets. It keeps the filename as a label.
        DebugLog::log("[homebrew] no asset section, using filename: %s", out.name.c_str());
        return true;
    }

    if (asset.nacp.size >= sizeof(NacpStruct)) {
        NacpStruct nacp{};
        if (readAt(f, assetOffset + (long)asset.nacp.offset, &nacp, sizeof(nacp))) {
            std::string name, author;
            if (firstNamedLanguage(nacp, name, author)) {
                out.name = std::move(name);
                out.author = std::move(author);
            }
        }
    }

    DebugLog::log("[homebrew] %s -> name='%s' author='%s' nacp=%llu icon=%llu",
                  std::filesystem::path(path).filename().string().c_str(),
                  out.name.c_str(), out.author.c_str(),
                  (unsigned long long)asset.nacp.size,
                  (unsigned long long)asset.icon.size);

    if (asset.icon.size > 0) {
        out.iconOffset = (std::uint64_t)assetOffset + asset.icon.offset;
        out.iconSize = (std::uint32_t)std::min<std::uint64_t>(asset.icon.size, 512u * 1024u);
    }

    return true;
}

void scanInto(const std::string& dir, int depth, int maxDepth, std::vector<Entry>& out) {
    if (depth > maxDepth)
        return;

    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
        return;

    for (const auto& item : it) {
        std::error_code itemEc;
        if (item.is_directory(itemEc)) {
            scanInto(item.path().string(), depth + 1, maxDepth, out);
            continue;
        }

        std::string p = item.path().string();
        if (p.size() < 4)
            continue;
        std::string ext = p.substr(p.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (ext != ".nro")
            continue;

        Entry entry;
        if (readEntry(p, entry))
            out.push_back(std::move(entry));
    }
}

}  // namespace

std::vector<Entry> scan(const std::string& root, int maxDepth) {
    // The walk reads a header per file off the SD card, and startup time is the
    // thing this project has spent the most effort protecting, so it is timed.
    const u64 startTick = armGetSystemTick();

    std::vector<Entry> out;
    scanInto(root, 0, maxDepth, out);

    std::sort(out.begin(), out.end(), [](const Entry& a, const Entry& b) {
        return a.name < b.name;
    });

    const unsigned ms =
        (unsigned)(armTicksToNs(armGetSystemTick() - startTick) / 1000000ULL);
    int withIcon = 0, withName = 0;
    for (const auto& e : out) {
        if (e.iconSize > 0) ++withIcon;
        if (!e.author.empty()) ++withName;
    }
    DebugLog::log("[homebrew] scan %s: %d entries, %d with icon, %d with NACP, %ums",
                  root.c_str(), (int)out.size(), withIcon, withName, ms);
    return out;
}

bool readIcon(const Entry& entry, std::vector<std::uint8_t>& out) {
    if (entry.iconSize == 0)
        return false;

    std::FILE* f = std::fopen(entry.path.c_str(), "rb");
    if (!f)
        return false;
    struct Closer {
        std::FILE* f;
        ~Closer() { if (f) std::fclose(f); }
    } closer{f};

    out.resize(entry.iconSize);
    const bool ok = readAt(f, (long)entry.iconOffset, out.data(), out.size());
    // A JPEG starts FFD8. If this is not one, the streamer will fail to decode
    // and the tile will be blank, so the bytes are worth naming here.
    const bool jpeg = ok && out.size() >= 2 && out[0] == 0xFF && out[1] == 0xD8;
    if (!ok || !jpeg)
        DebugLog::log("[homebrew] icon read %s for %s (%u bytes, jpeg=%d)",
                      ok ? "ok" : "FAILED", entry.name.c_str(),
                      (unsigned)out.size(), jpeg ? 1 : 0);
    return ok;
}

}  // namespace homebrew
