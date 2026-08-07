#pragma once
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

    // Lists the .nro files on the card alongside installed titles. Off by
    // default: it adds a scan of the card at startup, and an install that has
    // never asked for it should not pay for it.
    bool showHomebrew = false;
    // Listing homebrew is harmless; launching it takes over a system applet,
    // so the two are separate switches and this one starts off.
    bool homebrewLaunchEnabled = false;
    // homebrew::AppletHost. Parental controls by default: most consoles never
    // set it up, and taking it leaves the Album gallery working.
    int  homebrewAppletHost = 0;

    // Dark by default. The light preset was the one that shipped, and the
    // request to change it came with a reason: the interface reads better
    // dark, and a first boot into white is a jolt on a handheld.
    std::string themePreset = "Default Dark";

    bool load();

    bool save() const;

    static constexpr const char* kConfigDir  = "sdmc:/config/SwitchU";
    static constexpr const char* kConfigPath = "sdmc:/config/SwitchU/config.json";
};
