#include "Config.hpp"
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <system_error>

namespace {

template <typename T>
void readJsonOpt(const nlohmann::json& j, const char* key, T& out) {
    auto it = j.find(key);
    if (it != j.end() && !it->is_null()) {
        try {
            out = it->get<T>();
        } catch (...) {
        }
    }
}

} // namespace

bool AppConfig::load() {
    // The previous copy is tried when the current one is missing or unreadable.
    // A crash used to cost the player every setting they had ever chosen --
    // including tutorialCompleted, which is why the tutorial greeted them again
    // after the theme crash. Losing the last change is acceptable; losing the
    // whole file is not.
    nlohmann::json j;
    bool parsed = false;
    for (const char* path : {kConfigPath, kBackupPath}) {
        std::ifstream f(path);
        if (!f.is_open()) continue;
        try {
            f >> j;
            parsed = true;
            break;
        } catch (...) {
            // Truncated or half-written: fall through to the backup.
        }
    }
    if (!parsed) return false;

    readJsonOpt(j, "musicEnabled", musicEnabled);
    readJsonOpt(j, "musicVolume", musicVolume);
    readJsonOpt(j, "sfxVolume", sfxVolume);
    readJsonOpt(j, "gridColumns", gridColumns);
    readJsonOpt(j, "gridRows", gridRows);
    readJsonOpt(j, "uiLanguageOverride", uiLanguageOverride);
    readJsonOpt(j, "soundPreset", soundPreset);
    readJsonOpt(j, "defaultProfileEnabled", defaultProfileEnabled);
    readJsonOpt(j, "defaultProfileUid", defaultProfileUid);
    readJsonOpt(j, "tutorialCompleted", tutorialCompleted);
    readJsonOpt(j, "lastUpdateCheckDay", lastUpdateCheckDay);
    readJsonOpt(j, "clockUse12Hour", clockUse12Hour);
    readJsonOpt(j, "backgroundBlur", backgroundBlur);
    readJsonOpt(j, "glassSharpness", glassSharpness);
    readJsonOpt(j, "backgroundSpeed", backgroundSpeed);
    readJsonOpt(j, "backgroundSpeedChosen", backgroundSpeedChosen);
    readJsonOpt(j, "accessibilityEnabled", accessibilityEnabled);
    readJsonOpt(j, "accessibilitySpeakHints", accessibilitySpeakHints);
    readJsonOpt(j, "accessibilitySpeakContextEveryFocus", accessibilitySpeakContextEveryFocus);
    readJsonOpt(j, "accessibilitySpeakPosition", accessibilitySpeakPosition);
    readJsonOpt(j, "accessibilitySpeechRate", accessibilitySpeechRate);
    readJsonOpt(j, "themePreset", themePreset);
    readJsonOpt(j, "lastPageTitleId", lastPageTitleId);
    readJsonOpt(j, "sortMode", sortMode);
    readJsonOpt(j, "lastOpenedSequence", lastOpenedSequence);
    lastOpened.clear();
    if (auto it = j.find("lastOpened"); it != j.end() && it->is_object()) {
        for (auto& [k, v] : it->items()) {
            if (!v.is_number_unsigned()) continue;
            lastOpened.emplace_back(std::strtoull(k.c_str(), nullptr, 16),
                                    v.get<std::uint64_t>());
        }
    }

    if (musicVolume < 0.f) musicVolume = 0.f;
    if (musicVolume > 1.f) musicVolume = 1.f;
    if (sfxVolume   < 0.f) sfxVolume   = 0.f;
    if (sfxVolume   > 1.f) sfxVolume   = 1.f;
    gridColumns = std::clamp(gridColumns, 3, 8);
    gridRows = std::clamp(gridRows, 2, 5);
    if (uiLanguageOverride.empty()) uiLanguageOverride = "auto";
    if (soundPreset.empty()) soundPreset = "wiiu";
    if (!defaultProfileEnabled) defaultProfileUid.clear();
    accessibilitySpeechRate = std::clamp(accessibilitySpeechRate, 120, 320);
    if (themePreset.empty()) themePreset = "Default Dark";

    return true;
}

bool AppConfig::save() const {
    std::error_code ec;
    std::filesystem::create_directory("sdmc:/config", ec);
    ec.clear();
    std::filesystem::create_directory(kConfigDir, ec);

    nlohmann::json j;
    j["musicEnabled"] = musicEnabled;
    j["musicVolume"] = musicVolume;
    j["sfxVolume"] = sfxVolume;
    j["gridColumns"] = std::clamp(gridColumns, 3, 8);
    j["gridRows"] = std::clamp(gridRows, 2, 5);
    j["uiLanguageOverride"] = uiLanguageOverride;
    j["soundPreset"] = soundPreset;
    j["defaultProfileEnabled"] = defaultProfileEnabled;
    j["defaultProfileUid"] = defaultProfileEnabled ? defaultProfileUid : std::string();
    j["tutorialCompleted"] = tutorialCompleted;
    j["lastUpdateCheckDay"] = lastUpdateCheckDay;
    j["clockUse12Hour"] = clockUse12Hour;
    j["backgroundBlur"] = backgroundBlur;
    j["glassSharpness"] = glassSharpness;
    j["backgroundSpeed"] = backgroundSpeed;
    j["backgroundSpeedChosen"] = backgroundSpeedChosen;
    j["accessibilityEnabled"] = accessibilityEnabled;
    j["accessibilitySpeakHints"] = accessibilitySpeakHints;
    j["accessibilitySpeakContextEveryFocus"] = accessibilitySpeakContextEveryFocus;
    j["accessibilitySpeakPosition"] = accessibilitySpeakPosition;
    j["accessibilitySpeechRate"] = std::clamp(accessibilitySpeechRate, 120, 320);
    j["themePreset"] = themePreset;
    j["lastPageTitleId"] = lastPageTitleId;
    j["sortMode"] = sortMode;
    j["lastOpenedSequence"] = lastOpenedSequence;
    {
        nlohmann::json opened = nlohmann::json::object();
        char key[17];
        for (const auto& e : lastOpened) {
            std::snprintf(key, sizeof(key), "%016llX", (unsigned long long)e.first);
            opened[key] = e.second;
        }
        j["lastOpened"] = std::move(opened);
    }

    // Written beside the real file and swapped in, never over it. Truncating
    // the live config and then dying mid-write is how a crash used to reset
    // every setting to its default: load() found a half-written file, failed to
    // parse it, and started from scratch. The staging file absorbs that risk.
    const std::string staging = std::string(kConfigPath) + ".part";

    {
        std::ofstream f(staging, std::ios::trunc);
        if (!f.is_open()) return false;
        f << j.dump(2);
        f.flush();
        if (!f.good()) {
            f.close();
            std::remove(staging.c_str());
            return false;
        }
    }

    // The outgoing copy becomes the backup rather than being deleted, so the
    // window in which neither file is complete costs nothing.
    std::remove(kBackupPath);
    std::rename(kConfigPath, kBackupPath);   // absent on the very first save
    if (std::rename(staging.c_str(), kConfigPath) != 0) {
        std::remove(staging.c_str());
        return false;
    }
    // No commit here: save() is submitted to the thread pool, so this ran on a
    // worker while the main thread was also writing. Committing an fs session
    // from two threads at once is its own hazard, and the author's build —
    // which does not corrupt — commits nowhere.
    return true;
}
