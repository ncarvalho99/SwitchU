#pragma once

#include <switch.h>

namespace switchu::menu {

// The daemon catalog covers first paint, so the menu does not need an NS
// session during process startup. Open it only when a direct management query
// is requested, then keep the single session until process exit.
Result ensureNsService(const char* reason);
void shutdownNsService();

} // namespace switchu::menu
