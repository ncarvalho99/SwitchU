#include "HblOverride.hpp"
#include "core/DebugLog.hpp"

#include <switch.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace homebrew {
namespace {

constexpr const char* kOverrideDir  = "sdmc:/atmosphere/config";
constexpr const char* kOverridePath = "sdmc:/atmosphere/config/override_config.ini";
constexpr const char* kBackupPath   = "sdmc:/atmosphere/config/override_config.ini.switchu.bak";
constexpr const char* kHblPath      = "sdmc:/atmosphere/hbl.nsp";
constexpr const char* kBundledHbl   = "sdmc:/switch/SwitchU/homebrew/hbl.nsp";

// Written next to the config when this created hbl.nsp, so disable() knows the
// file is ours to delete rather than someone else's install.
constexpr const char* kOwnedMarker  = "sdmc:/config/SwitchU/hbl_owned";

// The block is bracketed by comments so disable() removes exactly what enable()
// added, whatever anyone else has done to the file in between. Atmosphere's
// parser ignores ';' lines, so these are inert to it.
constexpr const char* kBegin = "; --- switchu:hbl begin (do not edit by hand) ---";
constexpr const char* kEnd   = "; --- switchu:hbl end ---";

bool exists(const char* path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

std::string readFile(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeFile(const char* path, const std::string& body, std::string& error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = std::string("could not write ") + path;
        return false;
    }
    out << body;
    out.flush();
    if (!out) {
        error = std::string("write failed on ") + path;
        return false;
    }
    return true;
}

bool copyFile(const char* from, const char* to, std::string& error) {
    std::string body = readFile(from);
    if (body.empty()) {
        error = std::string("missing or empty ") + from;
        return false;
    }
    return writeFile(to, body, error);
}

std::string trim(const std::string& s) {
    std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> splitLines(const std::string& body) {
    std::vector<std::string> out;
    std::string line;
    std::istringstream in(body);
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        out.push_back(std::move(line));
    }
    return out;
}

// Which program_id_N slots are already spoken for. Atmosphere accepts a bare
// "program_id" as slot 0, so that spelling counts too.
std::array<bool, 8> usedSlots(const std::vector<std::string>& lines) {
    std::array<bool, 8> used{};
    bool inHbl = false;
    for (const auto& raw : lines) {
        std::string line = trim(raw);
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;
        if (line[0] == '[') {
            inHbl = (line.find("hbl_config") != std::string::npos);
            continue;
        }
        if (!inHbl)
            continue;

        std::string key = trim(line.substr(0, line.find('=')));
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (key == "program_id") {
            used[0] = true;
        } else if (key.rfind("program_id_", 0) == 0 && key.size() == 12) {
            char d = key[11];
            if (d >= '0' && d <= '7')
                used[(std::size_t)(d - '0')] = true;
        }
    }
    return used;
}

std::pair<std::size_t, std::size_t> findBlock(const std::vector<std::string>& lines) {
    std::size_t begin = std::string::npos, end = std::string::npos;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (trim(lines[i]) == kBegin)
            begin = i;
        else if (trim(lines[i]) == kEnd && begin != std::string::npos) {
            end = i;
            break;
        }
    }
    return {begin, end};
}

} // namespace

std::uint64_t hostProgramId(AppletHost host) {
    return host == AppletHost::Album ? 0x010000000000100Dull
                                     : 0x0100000000001001ull;
}

const char* hostProgramIdText(AppletHost host) {
    return host == AppletHost::Album ? "010000000000100D" : "0100000000001001";
}

int hostAppletId(AppletHost host) {
    // AppletId_LibraryAppletPhotoViewer = 0x15, AppletId_LibraryAppletAuth = 0x0A.
    return host == AppletHost::Album ? 0x15 : 0x0A;
}

Status inspect() {
    Status st;
    st.hblPresent = exists(kHblPath);
    st.hblIsOurs  = exists(kOwnedMarker);

    std::vector<std::string> lines = splitLines(readFile(kOverridePath));
    auto [begin, end] = findBlock(lines);
    if (begin == std::string::npos || end == std::string::npos)
        return st;

    st.overrideActive = true;
    for (std::size_t i = begin; i < end; ++i) {
        if (lines[i].find(hostProgramIdText(AppletHost::Album)) != std::string::npos) {
            st.activeHost = AppletHost::Album;
            break;
        }
    }
    return st;
}

bool enable(AppletHost host, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(kOverrideDir, ec);

    // hbl.nsp already there belongs to whoever put it there. Ours only fills a
    // gap; overwriting could downgrade a newer or patched loader.
    if (!exists(kHblPath)) {
        if (!copyFile(kBundledHbl, kHblPath, error))
            return false;
        std::filesystem::create_directories("sdmc:/config/SwitchU", ec);
        std::string ignored;
        writeFile(kOwnedMarker, "nx-hbloader written by SwitchU\n", ignored);
        DebugLog::log("[hbl] wrote %s from the bundled copy", kHblPath);
    } else {
        DebugLog::log("[hbl] %s already present, left alone", kHblPath);
    }

    std::string original = readFile(kOverridePath);
    std::vector<std::string> lines = splitLines(original);

    // Start from a clean slate if a previous block is still in there.
    auto [oldBegin, oldEnd] = findBlock(lines);
    if (oldBegin != std::string::npos && oldEnd != std::string::npos)
        lines.erase(lines.begin() + (long)oldBegin, lines.begin() + (long)oldEnd + 1);

    auto used = usedSlots(lines);
    int slot = -1;
    for (int i = 0; i < 8; ++i) {
        if (!used[(std::size_t)i]) { slot = i; break; }
    }
    if (slot < 0) {
        error = "all eight hbl override slots are taken";
        return false;
    }

    if (!original.empty()) {
        if (!writeFile(kBackupPath, original, error))
            return false;
    }

    // Find [hbl_config], or create it. Insert at the end of that section.
    std::size_t insertAt = lines.size();
    bool haveSection = false;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::string line = trim(lines[i]);
        if (line.empty() || line[0] != '[')
            continue;
        if (line.find("hbl_config") != std::string::npos) {
            haveSection = true;
            insertAt = lines.size();
            for (std::size_t j = i + 1; j < lines.size(); ++j) {
                if (!trim(lines[j]).empty() && trim(lines[j])[0] == '[') {
                    insertAt = j;
                    break;
                }
            }
            break;
        }
    }

    std::vector<std::string> block;
    block.push_back(kBegin);
    if (!haveSection)
        block.push_back("[hbl_config]");
    block.push_back("program_id_" + std::to_string(slot) + "=" + hostProgramIdText(host));
    // The '!' means override by default; holding R during the launch gives the
    // real applet back. That is what keeps parental controls reachable rather
    // than switched off -- docs/homebrew-recovery.md says so in those words.
    block.push_back("override_key_" + std::to_string(slot) + "=!R");
    block.push_back(kEnd);

    lines.insert(lines.begin() + (long)insertAt, block.begin(), block.end());

    std::string body;
    for (const auto& l : lines)
        body += l + "\n";

    if (!writeFile(kOverridePath, body, error))
        return false;

    DebugLog::log("[hbl] slot %d -> %s", slot, hostProgramIdText(host));
    return true;
}

bool disable(std::string& error) {
    std::vector<std::string> lines = splitLines(readFile(kOverridePath));
    auto [begin, end] = findBlock(lines);
    if (begin != std::string::npos && end != std::string::npos) {
        lines.erase(lines.begin() + (long)begin, lines.begin() + (long)end + 1);
        std::string body;
        for (const auto& l : lines)
            body += l + "\n";
        // Nothing but our own block left means the file was ours to begin with.
        bool empty = trim(body).empty();
        std::error_code ec;
        if (empty)
            std::filesystem::remove(kOverridePath, ec);
        else if (!writeFile(kOverridePath, body, error))
            return false;
    }

    if (exists(kOwnedMarker)) {
        std::error_code ec;
        std::filesystem::remove(kHblPath, ec);
        std::filesystem::remove(kOwnedMarker, ec);
        DebugLog::log("[hbl] removed the hbl.nsp this wrote");
    }

    DebugLog::log("[hbl] override removed");
    return true;
}

} // namespace homebrew
