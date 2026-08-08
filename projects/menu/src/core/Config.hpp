#pragma once
#include <cstdint>
#include <string>

struct AppConfig {
    bool  musicEnabled = true;
    float musicVolume  = 0.4f;
    float sfxVolume    = 0.7f;
    int   gridColumns  = 5;
    int   gridRows     = 3;
    std::string uiLanguageOverride = "auto";
    std::string soundPreset = "wiiu";
    bool  defaultProfileEnabled = false;
    std::string defaultProfileUid;
    bool  tutorialCompleted = false;
    bool  clockUse12Hour = false;
    bool  accessibilityEnabled = true;
    bool  accessibilitySpeakHints = true;
    bool  accessibilitySpeakContextEveryFocus = false;
    bool  accessibilitySpeakPosition = true;
    int   accessibilitySpeechRate = 190;

    // Softens the wallpaper and the shapes drifting over it. Barely on: enough
    // to take the hard edge off the shapes without anyone noticing a blur, and
    // the value asked for after people looked at it on real hardware.
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

    std::string themePreset = "Default Dark";

    bool load();

    bool save() const;

    static constexpr const char* kConfigDir  = "sdmc:/config/SwitchU";
    static constexpr const char* kConfigPath = "sdmc:/config/SwitchU/config.json";
};
