#pragma once
#include <switchu/smi_protocol.hpp>
#include <switchu/smi_helpers.hpp>
#include <switchu/file_log.hpp>
#include "ecs.hpp"
#include <switch.h>
#include <cstdio>
#include <cstring>

namespace switchu::daemon::menu_la {

static constexpr AppletId kMenuAppletId = AppletId_LibraryAppletPhotoViewer;
static constexpr bool kEnableExternalContentLaunch = true;
static AppletHolder g_holder = {};
static bool g_active = false;
static bool g_holderCreated = false;
static bool g_externalRegistered = false;
static LibAppletExitReason g_lastExitReason = LibAppletExitReason_Normal;

inline bool hasHolder() {
    return g_holderCreated;
}

inline bool isActive() {
    return g_active && appletHolderActive(&g_holder);
}

inline Result create() {
    switchu::FileLog::log("[menu_la] create begin active=%d holderActive=%d",
                          g_active ? 1 : 0,
                          (g_active && appletHolderActive(&g_holder)) ? 1 : 0);
    Result rc = appletCreateLibraryApplet(&g_holder,
        kMenuAppletId, LibAppletMode_AllForeground);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[menu_la] CreateLibApplet id=0x%X FAIL: 0x%X",
                              (u32)kMenuAppletId, rc);
        return rc;
    }
    switchu::FileLog::log("[menu_la] create ok id=0x%X", (u32)kMenuAppletId);
    g_holderCreated = true;
    return 0;
}

inline void cleanupHolder() {
    if (g_holderCreated) {
        appletHolderClose(&g_holder);
        g_holderCreated = false;
    }
    g_active = false;
    if (g_externalRegistered) {
        switchu::daemon::unregisterExternalContent(switchu::smi::kMenuTakeoverProgramId);
        g_externalRegistered = false;
    }
}

inline void terminate();

inline Result prepare() {
    if (g_holderCreated) {
        switchu::FileLog::log("[menu_la] prepare requested while holder exists; terminating first");
        terminate();
    }
    if (kEnableExternalContentLaunch) {
        Result ecsRc = switchu::daemon::registerExternalContent(
            switchu::smi::kMenuTakeoverProgramId, "/switch/SwitchU/bin/menu");
        if (R_FAILED(ecsRc)) {
            switchu::FileLog::log("[menu_la] external content registration failed rc=0x%X", ecsRc);
            return ecsRc;
        }
        g_externalRegistered = true;
    }
    return create();
}

inline Result startPrepared(smi::MenuStartMode mode, const smi::SystemStatus& status) {
    if (!g_holderCreated) {
        Result rc = prepare();
        if (R_FAILED(rc))
            return rc;
    }

    g_lastExitReason = LibAppletExitReason_Normal;
    switchu::FileLog::log("[menu_la] start begin mode=%u status.running=%d status.suspended=0x%016lX",
                          static_cast<u32>(mode),
                          status.app_running ? 1 : 0,
                          status.suspended_app_id);
    LibAppletArgs la_args;
    libappletArgsCreate(&la_args, static_cast<u32>(mode));
    libappletArgsSetPlayStartupSound(&la_args, true);
    Result rc = libappletArgsPush(&la_args, &g_holder);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[menu_la] ArgsPush FAIL: 0x%X", rc);
        cleanupHolder();
        return rc;
    }

    rc = libappletPushInData(&g_holder, &status, sizeof(status));
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[menu_la] PushStatus FAIL: 0x%X", rc);
    } else {
        switchu::FileLog::log("[menu_la] PushStatus ok size=%zu", sizeof(status));
    }

    switchu::FileLog::log("[menu_la] holder start call");
    rc = appletHolderStart(&g_holder);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[menu_la] Start FAIL: 0x%X", rc);
        cleanupHolder();
        return rc;
    }

    g_active = true;
    switchu::FileLog::log("[menu_la] started (mode=%u holderActive=%d)",
                          static_cast<u32>(mode),
                          appletHolderActive(&g_holder) ? 1 : 0);
    return 0;
}

inline Result launch(smi::MenuStartMode mode, const smi::SystemStatus& status) {
    Result rc = prepare();
    if (R_FAILED(rc)) return rc;
    return startPrepared(mode, status);
}

inline void terminate() {
    if (!g_holderCreated) return;
    switchu::FileLog::log("[menu_la] terminate begin holderActive=%d",
                          (g_active && appletHolderActive(&g_holder)) ? 1 : 0);
    if (g_active) {
        Result rc = appletHolderRequestExitOrTerminate(&g_holder, 15'000'000'000ULL);
        switchu::FileLog::log("[menu_la] terminate request rc=0x%X", rc);
        g_lastExitReason = R_SUCCEEDED(rc) ? appletHolderGetExitReason(&g_holder)
                                           : LibAppletExitReason_Unexpected;
    }
    cleanupHolder();
    switchu::FileLog::log("[menu_la] terminate done reason=%d", (int)g_lastExitReason);
}

inline bool checkFinished() {
    if (!g_active) return false;
    if (appletHolderCheckFinished(&g_holder)) {
        g_lastExitReason = appletHolderGetExitReason(&g_holder);
        switchu::FileLog::log("[menu_la] holder finished reason=%d",
                              (int)g_lastExitReason);
        appletHolderJoin(&g_holder);
        cleanupHolder();
        return true;
    }
    return false;
}

inline LibAppletExitReason exitReason() {
    if (g_active)
        return appletHolderGetExitReason(&g_holder);
    return g_lastExitReason;
}

inline Result pushStorage(AppletStorage* st) {
    Result rc = appletHolderPushInteractiveInData(&g_holder, st);
    if (R_FAILED(rc))
        switchu::FileLog::log("[menu_la] PushInteractiveInData FAIL: 0x%X", rc);
    appletStorageClose(st);
    return rc;
}

inline Result popStorage(AppletStorage* st) {
    return appletHolderPopInteractiveOutData(&g_holder, st);
}

}
