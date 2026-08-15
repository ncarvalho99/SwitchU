#pragma once
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>
#include <string>

struct AppConfig {
    bool  musicEnabled = true;
    // Levels asked for after people used the menu on real hardware: the music
    // sits under the game audio rather than competing with it, and the effects
    // stay audible above it. Both were louder before and were turned down by
    // nearly everyone who was asked.
    float musicVolume  = 0.15f;
    float sfxVolume    = 0.25f;
    int   gridColumns  = 5;
    int   gridRows     = 3;
    std::string uiLanguageOverride = "auto";
    std::string soundPreset = "wiiu";
    bool  defaultProfileEnabled = false;
    std::string defaultProfileUid;
    bool  tutorialCompleted = false;
    // Day number of the last update check, so the console asks GitHub once a
    // day rather than on every return from a game.
    int   lastUpdateCheckDay = 0;
    bool  clockUse12Hour = false;
    bool  accessibilityEnabled = true;
    bool  accessibilitySpeakHints = true;
    bool  accessibilitySpeakContextEveryFocus = false;
    bool  accessibilitySpeakPosition = true;
    int   accessibilitySpeechRate = 190;

    // Softens the wallpaper and the shapes drifting over it.
    // This was zero on the argument that the blur costs half the wallpaper's
    // resolution and 0.05 bought an invisible smudge in return. That argument
    // was made before the blur curve was reworked: the threshold that swallowed
    // the first 15% is gone and the low end is now a gentle, visible softening.
    // Users looked at the result on hardware and asked for it on by default.
    float backgroundBlur = 0.05f;

    // How sharply the scene reads through the glass panels. Capturing that
    // scene at full resolution made it sharper than it had ever been, which
    // helps a light theme and is too much on a dark one. 0.4 reproduces the
    // half-resolution look this replaced, and is the default for that reason.
    float glassSharpness = 0.4f;

    // Pace of the drifting shapes, as a slider position: 0 stops them, 0.5 is
    // the speed the theme asked for, 1 doubles it. Below centre by default --
    // the theme's own pace read as restless behind a menu people sit in.
    float backgroundSpeed = 0.35f;

    // Whether the slider above was ever moved by hand. An animated theme runs
    // at the speed of its clip until it was, because 0.35 is a default chosen
    // for shapes and would be a wrong answer for footage rather than a taste.
    bool backgroundSpeedChosen = false;

    // Dark by default. The light preset was the one that shipped, and the
    // request to change it came with a reason: the interface reads better
    // dark, and a first boot into white is a jolt on a handheld.
    // A title from the page the menu was last on. Returning from a suspended
    // game already jumps to it, but closing a game outright, or opening an
    // applet, suspends nothing -- and the menu came back on page one every
    // time. Stored as a title rather than a page number because the grid can
    // be rebuilt with a different width between the two moments.
    std::uint64_t lastPageTitleId = 0;

    // 0 = the arrangement the owner made by hand, which stays the default:
    // somebody who dragged their icons into an order did not do that to have
    // it thrown away. 1 = A to Z. 2 = most recently opened first.
    int sortMode = 0;

    // When each title was last opened, by title id. The record ns keeps is
    // last_updated -- when it was installed or patched -- which is not the
    // same question and puts a game patched this morning above one played
    // every day for a year.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> lastOpened;
    // A persisted sequence rather than a system/boot clock. The console can
    // start without a valid wall clock and armGetSystemTick resets on reboot;
    // neither can provide a reliable "most recently opened" order.
    std::uint64_t lastOpenedSequence = 0;

    std::uint64_t lastOpenedAt(std::uint64_t titleId) const {
        for (const auto& e : lastOpened)
            if (e.first == titleId) return e.second;
        return 0;
    }
    void noteOpened(std::uint64_t titleId, std::uint64_t when) {
        for (auto& e : lastOpened) {
            if (e.first == titleId) { e.second = when; return; }
        }
        lastOpened.emplace_back(titleId, when);
    }
    std::uint64_t nextLastOpenedAt() {
        for (const auto& e : lastOpened)
            if (e.second > lastOpenedSequence)
                lastOpenedSequence = e.second;
        if (lastOpenedSequence != std::numeric_limits<std::uint64_t>::max())
            ++lastOpenedSequence;
        return lastOpenedSequence;
    }

    std::string themePreset = "Default Dark";

    bool load();

    bool save() const;

    static constexpr const char* kConfigDir  = "sdmc:/config/SwitchU";
    static constexpr const char* kConfigPath = "sdmc:/config/SwitchU/config.json";
    // The copy that was current before the last save. Read only when
    // config.json is missing or does not parse.
    static constexpr const char* kBackupPath = "sdmc:/config/SwitchU/config.json.bak";
};
