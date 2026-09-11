#include "GameCheatManager.hpp"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace cheats {
namespace {

bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string trim(const std::string& s) {
    const auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool isDirectiveOrSeparator(const std::string& name) {
    if (name.empty()) return true;
    if (name.rfind("--Section", 0) == 0 ||
        name.rfind("--section", 0) == 0 ||
        name.rfind("--Disable", 0) == 0 ||
        name.rfind("--disable", 0) == 0) {
        return true;
    }
    bool onlySymbols = true;
    for (char c : name) {
        if (c != '-' && c != '=' && c != ' ' && c != '_' && c != '*' && c != '#') {
            onlySymbols = false;
            break;
        }
    }
    return onlySymbols;
}

std::unordered_map<std::string, bool> loadToggleMap(const std::filesystem::path& toggleFile) {
    std::unordered_map<std::string, bool> toggles;
    std::error_code ec;
    if (!std::filesystem::exists(toggleFile, ec) || ec) {
        return toggles;
    }

    std::ifstream stream(toggleFile);
    if (!stream.is_open()) {
        return toggles;
    }

    std::string line;
    std::string currentCheatName;
    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        if (trimmed.front() == '[' && trimmed.back() == ']') {
            currentCheatName = trimmed.substr(1, trimmed.size() - 2);
        } else if (!currentCheatName.empty()) {
            const bool enabled = equalsIgnoreCase(trimmed, "true") ||
                                 trimmed == "1" ||
                                 equalsIgnoreCase(trimmed, "on");
            toggles[currentCheatName] = enabled;
            currentCheatName.clear();
        }
    }

    return toggles;
}

} // namespace

std::filesystem::path GameCheatManager::titleRoot(std::uint64_t titleId) {
    const std::filesystem::path base("sdmc:/atmosphere/contents");
    const std::string upperHex = fmt::format("{:016X}", titleId);
    const std::string lowerHex = fmt::format("{:016x}", titleId);

    std::error_code ec;
    const auto upperPath = base / upperHex;
    if (std::filesystem::is_directory(upperPath, ec) && !ec) {
        return upperPath;
    }

    ec.clear();
    const auto lowerPath = base / lowerHex;
    if (std::filesystem::is_directory(lowerPath, ec) && !ec) {
        return lowerPath;
    }

    return upperPath;
}

std::filesystem::path GameCheatManager::cheatsPath(std::uint64_t titleId) {
    const auto root = titleRoot(titleId);
    std::error_code ec;
    const auto standardCheats = root / "cheats";
    if (std::filesystem::is_directory(standardCheats, ec) && !ec) {
        return standardCheats;
    }

    ec.clear();
    const auto disabledCheats = root / ".switchu-disabled" / "cheats";
    if (std::filesystem::is_directory(disabledCheats, ec) && !ec) {
        return disabledCheats;
    }

    return standardCheats;
}

bool GameCheatManager::hasCheats(std::uint64_t titleId) {
    const auto dir = cheatsPath(titleId);
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec) {
        return false;
    }

    for (const auto& item : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec) || ec) continue;
        const auto filename = item.path().filename().string();
        if (equalsIgnoreCase(filename, "toggles.txt")) continue;
        if (item.path().extension() == ".txt") {
            return true;
        }
    }

    return false;
}

std::vector<CheatCode> GameCheatManager::parseCheatFile(const std::string& content) {
    std::vector<CheatCode> cheats;
    std::istringstream stream(content);
    std::string line;
    CheatCode current;
    bool inCheat = false;
    bool isFirstLine = true;

    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Strip UTF-8 BOM if present on initial line
        if (isFirstLine) {
            isFirstLine = false;
            if (trimmed.size() >= 3 &&
                static_cast<unsigned char>(trimmed[0]) == 0xEF &&
                static_cast<unsigned char>(trimmed[1]) == 0xBB &&
                static_cast<unsigned char>(trimmed[2]) == 0xBF) {
                trimmed = trim(trimmed.substr(3));
                if (trimmed.empty()) continue;
            }
        }

        // Detect cheat headers: [Name] for standard cheats, {Name} for master cheats
        if (trimmed.front() == '[' || trimmed.front() == '{') {
            const char closeChar = (trimmed.front() == '[') ? ']' : '}';
            const size_t endPos = trimmed.find(closeChar);
            if (endPos != std::string::npos && endPos > 1) {
                if (inCheat && (!current.lines.empty() || current.isMaster)) {
                    if (!isDirectiveOrSeparator(current.name)) {
                        cheats.push_back(std::move(current));
                    }
                    current = CheatCode{};
                }
                std::string headerName = trim(trimmed.substr(1, endPos - 1));
                bool defaultOn = false;
                const std::string enabledSuffix = ":ENABLED";
                if (headerName.size() > enabledSuffix.size() &&
                    equalsIgnoreCase(headerName.substr(headerName.size() - enabledSuffix.size()), enabledSuffix)) {
                    headerName = trim(headerName.substr(0, headerName.size() - enabledSuffix.size()));
                    defaultOn = true;
                }
                current.name = headerName;
                current.isMaster = (closeChar == '}');
                current.defaultEnabled = defaultOn;
                current.enabled = false;
                inCheat = true;
                continue;
            }
        }

        // Ignore comments
        if (trimmed.rfind("//", 0) == 0 || trimmed.front() == ';' || trimmed.front() == '#') {
            continue;
        }

        if (inCheat) {
            current.lines.push_back(trimmed);
        }
    }

    if (inCheat && (!current.lines.empty() || current.isMaster)) {
        if (!isDirectiveOrSeparator(current.name)) {
            cheats.push_back(std::move(current));
        }
    }

    return cheats;
}

std::vector<BuildCheats> GameCheatManager::load(std::uint64_t titleId) {
    std::vector<BuildCheats> builds;
    const auto dir = cheatsPath(titleId);
    std::error_code ec;

    if (!std::filesystem::is_directory(dir, ec) || ec) {
        return builds;
    }

    const auto toggleFile = dir / "toggles.txt";
    const auto toggles = loadToggleMap(toggleFile);

    std::vector<std::filesystem::path> cheatFiles;
    for (const auto& item : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!item.is_regular_file(ec) || ec) continue;
        const auto filename = item.path().filename().string();
        if (equalsIgnoreCase(filename, "toggles.txt")) continue;
        if (item.path().extension() == ".txt") {
            cheatFiles.push_back(item.path());
        }
    }

    std::sort(cheatFiles.begin(), cheatFiles.end());

    for (const auto& path : cheatFiles) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) continue;

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();

        auto parsed = parseCheatFile(content);
        if (parsed.empty()) continue;

        for (auto& cheat : parsed) {
            auto it = toggles.find(cheat.name);
            if (it == toggles.end()) {
                for (auto mapIt = toggles.begin(); mapIt != toggles.end(); ++mapIt) {
                    if (equalsIgnoreCase(mapIt->first, cheat.name)) {
                        it = mapIt;
                        break;
                    }
                }
            }
            if (it != toggles.end()) {
                cheat.enabled = it->second;
            } else {
                cheat.enabled = cheat.isMaster || cheat.defaultEnabled;
            }
        }

        BuildCheats build;
        build.buildId = path.stem().string();
        build.filePath = path;
        build.cheats = std::move(parsed);
        builds.push_back(std::move(build));
    }

    return builds;
}

CheatResult GameCheatManager::saveToggles(std::uint64_t titleId, const std::vector<CheatCode>& cheats) {
    const auto dir = cheatsPath(titleId);
    std::error_code ec;

    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return {false, "Failed to create cheats directory: " + ec.message()};
    }

    const auto toggleFile = dir / "toggles.txt";
    auto allToggles = loadToggleMap(toggleFile);

    for (const auto& cheat : cheats) {
        if (!cheat.isMaster) {
            allToggles[cheat.name] = cheat.enabled;
        }
    }

    std::ofstream out(toggleFile, std::ios::trunc);
    if (!out.is_open()) {
        return {false, "Failed to open toggles.txt for writing"};
    }

    for (const auto& [name, enabled] : allToggles) {
        out << "[" << name << "]\n" << (enabled ? "true\n" : "false\n");
    }
    out.flush();

    return {true, ""};
}

} // namespace cheats
