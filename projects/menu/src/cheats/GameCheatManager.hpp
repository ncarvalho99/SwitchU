#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cheats {

struct CheatCode {
    std::string name;
    std::vector<std::string> lines;
    bool enabled = false;
    bool isMaster = false;
};

struct BuildCheats {
    std::string buildId;
    std::filesystem::path filePath;
    std::vector<CheatCode> cheats;
};

struct CheatResult {
    bool ok = false;
    std::string error;
};

class GameCheatManager {
public:
    static std::filesystem::path titleRoot(std::uint64_t titleId);
    static std::filesystem::path cheatsPath(std::uint64_t titleId);
    static std::vector<BuildCheats> load(std::uint64_t titleId);
    static CheatResult saveToggles(std::uint64_t titleId, const std::vector<CheatCode>& cheats);
    static bool hasCheats(std::uint64_t titleId);
    static std::vector<CheatCode> parseCheatFile(const std::string& content);
};

} // namespace cheats
