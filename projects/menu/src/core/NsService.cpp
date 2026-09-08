#include "NsService.hpp"

#include "DebugLog.hpp"

namespace {

Mutex g_nsMutex{};
bool g_nsInitialized = false;

} // namespace

namespace switchu::menu {

Result ensureNsService(const char* reason) {
    mutexLock(&g_nsMutex);
    if (g_nsInitialized) {
        mutexUnlock(&g_nsMutex);
        return 0;
    }

    const Result rc = nsInitialize();
    if (R_SUCCEEDED(rc))
        g_nsInitialized = true;
    mutexUnlock(&g_nsMutex);

    DebugLog::log("[ns] lazy initialize reason=%s rc=0x%X",
                  reason ? reason : "unknown", rc);
    return rc;
}

void shutdownNsService() {
    mutexLock(&g_nsMutex);
    const bool wasInitialized = g_nsInitialized;
    if (wasInitialized) {
        nsExit();
        g_nsInitialized = false;
    }
    mutexUnlock(&g_nsMutex);
}

} // namespace switchu::menu
