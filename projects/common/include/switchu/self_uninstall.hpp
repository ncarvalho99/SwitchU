#pragma once

namespace switchu::self_uninstall {

// Marker written last by the menu after the player confirms removal. The daemon
// consumes it at the next boot, before it launches an external menu payload.
inline constexpr const char* kDirectory = "sdmc:/config/SwitchU/uninstall";
inline constexpr const char* kRequest = "sdmc:/config/SwitchU/uninstall/request";
inline constexpr const char* kRequestTemporary = "sdmc:/config/SwitchU/uninstall/request.tmp";
inline constexpr char kRequestContents[] = "SwitchU self-uninstall request v1\n";

// SD card paths completely removed when self-uninstall is applied.
inline constexpr const char* kOverrideDirectory =
    "sdmc:/atmosphere/contents/0100000000001000";
inline constexpr const char* kMenuDirectory =
    "sdmc:/switch/SwitchU";
inline constexpr const char* kMenuNro =
    "sdmc:/switch/SwitchU.nro";
inline constexpr const char* kManagerDirectory =
    "sdmc:/switch/SwitchU-Manager";
inline constexpr const char* kManagerNro =
    "sdmc:/switch/SwitchU-Manager.nro";
inline constexpr const char* kConfigDirectory =
    "sdmc:/config/SwitchU";

} // namespace switchu::self_uninstall
