#include "HomebrewScanner.hpp"
#include "core/DebugLog.hpp"

#include <switch.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <utility>
#include <vector>

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

// strnlen is POSIX and does not arrive through <cstring> here, and these fields
// are fixed-size and not guaranteed terminated, so the bound is explicit.
std::size_t boundedLength(const char* p, std::size_t max) {
    std::size_t n = 0;
    while (n < max && p[n] != 0)
        ++n;
    return n;
}

// A NACP holds sixteen language entries and the console's language is not known
// here, so the first non-empty one is used. hbmenu does the same, which keeps
// the two lists reading alike.
bool firstNamedLanguage(const NacpStruct& nacp, std::string& name, std::string& author) {
    for (const auto& lang : nacp.lang) {
        if (lang.name[0] == '\0')
            continue;
        name.assign(lang.name, boundedLength(lang.name, sizeof(lang.name)));
        author.assign(lang.author, boundedLength(lang.author, sizeof(lang.author)));
        return true;
    }
    return false;
}

bool readEntryImpl(const std::string& path, Entry& out) {
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

// Measured on a real card: 42 NROs among 871 files, 303ms to walk and 455ms to
// open and read their headers. The reads are what a cache can remove, keyed by
// the size and modification time the walk already has in hand, so an unchanged
// file is never opened twice across boots.
constexpr const char* kCachePath = "sdmc:/config/SwitchU/homebrew.bin";
constexpr std::uint32_t kCacheMagic = 0x4257424Eu;  // 'NBWB'
constexpr std::uint32_t kCacheVersion = 1;

struct CacheKey {
    std::uint64_t size = 0;
    std::int64_t mtime = 0;
};

void writeStr(std::FILE* f, const std::string& s) {
    std::uint32_t n = (std::uint32_t)s.size();
    std::fwrite(&n, sizeof(n), 1, f);
    if (n) std::fwrite(s.data(), 1, n, f);
}

bool readStr(std::FILE* f, std::string& out) {
    std::uint32_t n = 0;
    if (std::fread(&n, sizeof(n), 1, f) != 1) return false;
    if (n > 4096) return false;              // a path or a name, never this long
    out.assign(n, '?');
    return n == 0 || std::fread(&out[0], 1, n, f) == n;
}

struct CachedRecord {
    CacheKey key;
    Entry entry;
};

void loadCacheImpl(std::vector<std::pair<std::string, CachedRecord>>& out) {
    std::FILE* f = std::fopen(kCachePath, "rb");
    if (!f)
        return;
    struct Closer { std::FILE* f; ~Closer() { if (f) std::fclose(f); } } closer{f};

    std::uint32_t magic = 0, version = 0, count = 0;
    if (std::fread(&magic, sizeof(magic), 1, f) != 1) return;
    if (std::fread(&version, sizeof(version), 1, f) != 1) return;
    if (std::fread(&count, sizeof(count), 1, f) != 1) return;
    if (magic != kCacheMagic || version != kCacheVersion || count > 4096)
        return;

    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::string path;
        CachedRecord rec;
        if (!readStr(f, path)) return;
        if (std::fread(&rec.key.size, sizeof(rec.key.size), 1, f) != 1) return;
        if (std::fread(&rec.key.mtime, sizeof(rec.key.mtime), 1, f) != 1) return;
        if (!readStr(f, rec.entry.name)) return;
        if (!readStr(f, rec.entry.author)) return;
        if (std::fread(&rec.entry.iconOffset, sizeof(rec.entry.iconOffset), 1, f) != 1) return;
        if (std::fread(&rec.entry.iconSize, sizeof(rec.entry.iconSize), 1, f) != 1) return;
        rec.entry.path = path;
        out.emplace_back(std::move(path), std::move(rec));
    }
}

void saveCacheImpl(const std::vector<std::pair<std::string, CacheKey>>& paths,
                   const std::vector<Entry>& entries) {
    std::error_code ec;
    std::filesystem::create_directories("sdmc:/config/SwitchU", ec);

    std::FILE* f = std::fopen(kCachePath, "wb");
    if (!f)
        return;
    struct Closer { std::FILE* f; ~Closer() { if (f) std::fclose(f); } } closer{f};

    std::uint32_t magic = kCacheMagic, version = kCacheVersion;
    std::uint32_t count = (std::uint32_t)entries.size();
    std::fwrite(&magic, sizeof(magic), 1, f);
    std::fwrite(&version, sizeof(version), 1, f);
    std::fwrite(&count, sizeof(count), 1, f);

    for (const auto& e : entries) {
        CacheKey key;
        for (const auto& p : paths) {
            if (p.first == e.path) { key = p.second; break; }
        }
        writeStr(f, e.path);
        std::fwrite(&key.size, sizeof(key.size), 1, f);
        std::fwrite(&key.mtime, sizeof(key.mtime), 1, f);
        writeStr(f, e.name);
        writeStr(f, e.author);
        std::fwrite(&e.iconOffset, sizeof(e.iconOffset), 1, f);
        std::fwrite(&e.iconSize, sizeof(e.iconSize), 1, f);
    }
}

// Timing split: the walk and the per-file header reads are separate costs with
// separate fixes, and 751ms told me the total without saying which.
struct ScanCost {
    unsigned walkMs = 0;
    unsigned readMs = 0;
    int filesSeen = 0;
    int reused = 0;
};

void collectPaths(const std::string& dir, int depth, int maxDepth,
                  std::vector<std::pair<std::string, CacheKey>>& out, ScanCost& cost) {
    if (depth > maxDepth)
        return;

    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
        return;

    for (const auto& item : it) {
        ++cost.filesSeen;
        std::error_code itemEc;
        if (item.is_directory(itemEc)) {
            collectPaths(item.path().string(), depth + 1, maxDepth, out, cost);
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

        // Both come from the directory entry the walk already produced, so
        // they cost nothing beyond what finding the file cost.
        CacheKey key;
        std::error_code sizeEc, timeEc;
        key.size = (std::uint64_t)item.file_size(sizeEc);
        key.mtime = (std::int64_t)item.last_write_time(timeEc)
                        .time_since_epoch().count();
        if (sizeEc || timeEc)
            key = CacheKey{};
        out.emplace_back(std::move(p), key);
    }
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
        if (readEntryImpl(p, entry))
            out.push_back(std::move(entry));
    }
}

}  // namespace

std::vector<Entry> scan(const std::string& root, int maxDepth) {
    // The walk reads a header per file off the SD card, and startup time is the
    // thing this project has spent the most effort protecting, so it is timed.
    const u64 startTick = armGetSystemTick();

    ScanCost cost;
    std::vector<std::pair<std::string, CacheKey>> paths;
    const u64 walkStart = armGetSystemTick();
    collectPaths(root, 0, maxDepth, paths, cost);
    cost.walkMs = (unsigned)(armTicksToNs(armGetSystemTick() - walkStart) / 1000000ULL);

    std::vector<std::pair<std::string, CachedRecord>> cache;
    loadCacheImpl(cache);

    const u64 readStart = armGetSystemTick();
    std::vector<Entry> out;
    out.reserve(paths.size());
    int reused = 0;
    for (auto& pathAndKey : paths) {
        const std::string& path = pathAndKey.first;
        const CacheKey& key = pathAndKey.second;

        const CachedRecord* hit = nullptr;
        if (key.size != 0) {
            for (const auto& c : cache) {
                if (c.first == path && c.second.key.size == key.size &&
                    c.second.key.mtime == key.mtime) {
                    hit = &c.second;
                    break;
                }
            }
        }

        if (hit) {
            out.push_back(hit->entry);
            ++reused;
            continue;
        }

        Entry entry;
        if (readEntryImpl(path, entry))
            out.push_back(std::move(entry));
    }
    cost.readMs = (unsigned)(armTicksToNs(armGetSystemTick() - readStart) / 1000000ULL);
    cost.reused = reused;

    if (reused != (int)out.size())
        saveCacheImpl(paths, out);

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
    DebugLog::log("[homebrew] scan %s: %d entries of %d files, %d icon, %d nacp, "
                  "%ums total (walk %ums, headers %ums, %d from cache)",
                  root.c_str(), (int)out.size(), cost.filesSeen, withIcon, withName,
                  ms, cost.walkMs, cost.readMs, cost.reused);
    return out;
}

bool readOne(const std::string& path, Entry& out) {
    return readEntryImpl(path, out);
}

std::uint64_t syntheticTitleId(const std::string& path) {
    // FNV-1a. Any stable hash would do; what matters is bit 63, which keeps
    // these out of the range real title ids occupy.
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : path) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h | (1ull << 63);
}

namespace {
std::vector<std::pair<std::uint64_t, std::string>>& pathRegistry() {
    static std::vector<std::pair<std::uint64_t, std::string>> registry;
    return registry;
}
}

void registerPath(std::uint64_t id, std::string path) {
    auto& reg = pathRegistry();
    for (auto& e : reg) {
        if (e.first == id) {
            e.second = std::move(path);
            return;
        }
    }
    reg.emplace_back(id, std::move(path));
}

bool pathForId(std::uint64_t id, std::string& out) {
    for (const auto& e : pathRegistry()) {
        if (e.first == id) {
            out = e.second;
            return true;
        }
    }
    return false;
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
