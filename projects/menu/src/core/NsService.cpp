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

    const uint64_t startTick = armGetSystemTick();
    const Result rc = nsInitialize();
    const uint64_t elapsedUs = armTicksToNs(armGetSystemTick() - startTick) / 1000ULL;
    if (R_SUCCEEDED(rc))
        g_nsInitialized = true;
    mutexUnlock(&g_nsMutex);

    DebugLog::log("[ns] lazy initialize reason=%s rc=0x%X time_us=%llu",
                  reason ? reason : "unknown", rc,
                  static_cast<unsigned long long>(elapsedUs));
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

    if (wasInitialized)
        DebugLog::log("[ns] lazy session closed");
}

} // namespace switchu::menu
