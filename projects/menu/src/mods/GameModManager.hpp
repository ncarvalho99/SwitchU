#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mods {

enum class Kind { Romfs, Exefs, Cheats };

struct Entry {
    std::string name;
    Kind kind = Kind::Romfs;
    bool enabled = true;
    std::filesystem::path path;
};

struct Result {
    bool ok = false;
    std::string error;
};

// LayeredFS has no native enabled flag. SwitchU disables one discovered unit by
// moving it to a title-local staging directory that Atmosphere never loads.
// Every move stays on the SD card, so it is atomic on FAT/exFAT and preserves
// the original name for a later re-enable.
class GameModManager {
public:
    static std::vector<Entry> scan(std::uint64_t titleId);
    static Result toggle(const Entry& entry);
    static Result remove(const Entry& entry);
    static const char* kindDirectory(Kind kind);
};

} // namespace mods
