#pragma once

namespace switchu::daemon::self_uninstall {

enum class StagedRequestResult {
    None,
    Pending,
    Applied,
};

// Disables the exact qlaunch ExeFS override when the menu has staged a removal
// request. Runs at boot before the external menu payload is opened. A pending
// request blocks staged updates so they cannot restore an override the player
// asked to disable.
StagedRequestResult applyStagedRequest();

} // namespace switchu::daemon::self_uninstall
