#pragma once

#include <cstdint>
#include <string>

// Atmosphere runs homebrew by letting nx-hbloader take the place of a system
// applet. The mapping lives in a plain INI on the SD card, which is the whole
// reason this is recoverable without a computer: see docs/homebrew-recovery.md.
namespace homebrew {

// Verified against Atmosphere's own parser, not from memory:
// libstratosphere/source/cfg/cfg_override.board.nintendo_nx.inc reads
// program_id_0..7 and override_key_0..7, and holds a single global hbl path.
enum class AppletHost : int {
    ParentalControls = 0,   // 0100000000001001 "auth"
    Album            = 1,   // 010000000000100D "photoViewer"
};

std::uint64_t hostProgramId(AppletHost host);
const char*   hostProgramIdText(AppletHost host);

// The applet id the menu must start to reach hbl for a given host.
int hostAppletId(AppletHost host);

struct Status {
    bool       overrideActive = false;  // our program_id_N is in the file
    AppletHost activeHost     = AppletHost::ParentalControls;
    bool       hblPresent     = false;  // /atmosphere/hbl.nsp exists
    bool       hblIsOurs      = false;  // we put it there, so we may remove it
};

Status inspect();

// Adds our entry in the first free program_id slot, leaving every other line --
// including anyone else's overrides and the global path -- exactly as found.
// The previous file is copied to override_config.ini.switchu.bak first.
//
// Writes /atmosphere/hbl.nsp only when nothing is there: an existing one is
// very likely newer than what shipped here, and replacing it would silently
// downgrade a working setup.
bool enable(AppletHost host, std::string& error);

// Removes only the lines this wrote, and the hbl.nsp only if this created it.
// Turning the setting off is meant to be the whole recovery procedure.
bool disable(std::string& error);

} // namespace homebrew
