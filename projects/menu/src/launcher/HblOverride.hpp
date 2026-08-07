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
    bool       loaderPresent  = false;  // our loader is installed on the card
};

Status inspect();

// Adds our entry in the first free program_id slot, leaving everyone else's
// overrides as found, and points the global loader path at SwitchU's loader.
// The previous file goes to override_config.ini.switchu.bak, and the previous
// loader path is kept separately so disable() can hand it back.
//
// Nobody's hbl.nsp is overwritten: Atmosphere holds one loader path for the
// whole system, so ours is pointed at rather than copied over theirs. That
// also means this is the console's loader until it is turned off, which
// docs/homebrew-recovery.md states outright.
bool enable(AppletHost host, std::string& error);

// Removes only the lines this wrote and restores the loader path that was in
// force before. Turning the setting off is meant to be the whole recovery
// procedure.
bool disable(std::string& error);

} // namespace homebrew
