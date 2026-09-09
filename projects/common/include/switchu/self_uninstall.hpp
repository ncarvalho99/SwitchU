#pragma once

namespace switchu::self_uninstall {

// Marker written last by the menu after the player confirms removal. The daemon
// consumes it at the next boot, before it launches an external menu payload.
inline constexpr const char* kDirectory = "sdmc:/config/SwitchU/uninstall";
inline constexpr const char* kRequest = "sdmc:/config/SwitchU/uninstall/request";
inline constexpr const char* kRequestTemporary = "sdmc:/config/SwitchU/uninstall/request.tmp";
inline constexpr char kRequestContents[] = "SwitchU self-uninstall request v1\n";

// This is the one SwitchU file that needs to change for stock Nintendo HOME to
// return. Keep the original as .disabled so manual recovery remains possible.
inline constexpr const char* kActiveOverride =
    "sdmc:/atmosphere/contents/0100000000001000/exefs.nsp";
inline constexpr const char* kDisabledOverride =
    "sdmc:/atmosphere/contents/0100000000001000/exefs.nsp.disabled";

} // namespace switchu::self_uninstall
