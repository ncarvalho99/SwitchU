#pragma once

namespace switchu::daemon::update {

// Puts a staged launcher update in place, if the menu left one ready.
//
// Called once at boot, before the menu is launched: that is the only moment at
// which none of the files being replaced is open. Does nothing when there is no
// staged update, and gives up after a few failed attempts rather than delaying
// every boot.
void applyStagedUpdate();

} // namespace switchu::daemon::update
