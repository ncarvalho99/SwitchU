#pragma once
#include <switch.h>
#include <switchu/file_log.hpp>
#include <cstdio>
#include <cstring>

namespace switchu::daemon::app {

static AppletApplication g_app = {};
static bool g_running = false;
static bool g_hasForeground = false;
static uint64_t g_suspendedTitleId = 0;
static bool g_lastLaunchAcceptsUser = true;
static bool g_lastLaunchNeedsUser = true;
static uint8_t g_lastStartupUserAccount = 1;
static uint8_t g_lastStartupUserAccountOption = 0;
// Save-data pre-creation mirrors qlaunch/ulaunch behaviour.
// The check-then-create path in ensureSaveData is safe: it tries to open first
// and only calls fsCreateSaveDataFileSystem when the filesystem does not exist.
static constexpr bool kEnableSaveDataEnsure = true;

inline bool isRunning() { return g_running; }
inline bool hasForeground() { return g_hasForeground; }
inline uint64_t suspendedTitleId() { return g_suspendedTitleId; }

inline bool startupUserRequiresInteractiveSelection(uint8_t account, uint8_t option) {
    return account == 1 && option == 0;
}

inline bool titleIdLooksStandardApplication(uint64_t title_id) {
    return (title_id & 0xFF00000000000000ULL) == 0x0100000000000000ULL;
}

inline bool looksLikeHomebrewForwarder(uint64_t title_id) {
    return title_id != 0 && !titleIdLooksStandardApplication(title_id);
}

inline bool suspendedTitleLooksLikeHomebrewForwarder() {
    return g_running && looksLikeHomebrewForwarder(g_suspendedTitleId);
}

static inline void ensureSaveData(uint64_t app_id, uint64_t owner_id,
                                  AccountUid user_id, FsSaveDataType type,
                                  FsSaveDataSpaceId space_id,
                                  uint64_t save_size, uint64_t journal_size) {
    if (save_size == 0) return;

    FsSaveDataAttribute attr = {};
    attr.application_id = app_id;
    attr.uid = user_id;
    attr.save_data_type = type;
    attr.save_data_rank = FsSaveDataRank_Primary;

    FsSaveDataCreationInfo cr = {};
    cr.save_data_size = (s64)save_size;
    cr.journal_size   = (s64)journal_size;
    cr.available_size = 0x4000;
    cr.owner_id       = owner_id;
    cr.save_data_space_id = (u8)space_id;

    FsSaveDataMetaInfo meta = {};
    if (type == FsSaveDataType_Bcat) {
        meta.size = 0;
        meta.type = FsSaveDataMetaType_None;
    } else {
        meta.size = 0x40060;
        meta.type = FsSaveDataMetaType_Thumbnail;
    }

    FsFileSystem fs;
    if (R_SUCCEEDED(fsOpenSaveDataFileSystem(&fs, space_id, &attr))) {
        fsFsClose(&fs);
        switchu::FileLog::log("[app] ensureSaveData type=%d already exists (ok)", (int)type);
    } else {
        switchu::FileLog::log("[app] ensureSaveData type=%d creating (size=0x%lX journal=0x%lX)",
                              (int)type, save_size, journal_size);
        Result rc = fsCreateSaveDataFileSystem(&attr, &cr, &meta);
        if (R_FAILED(rc))
            switchu::FileLog::log("[app] ensureSaveData type=%d FAIL: 0x%X", (int)type, rc);
        else
            switchu::FileLog::log("[app] ensureSaveData type=%d created ok", (int)type);
    }
}

static inline void ensureApplicationSaveData(uint64_t title_id, AccountUid uid) {
    NsApplicationControlData* ctrl = new NsApplicationControlData();
    if (!ctrl) return;

    u64 got_size = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage,
                                            title_id, ctrl, sizeof(*ctrl), &got_size);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[app] GetControlData FAIL: 0x%X (save data not created)", rc);
        delete ctrl;
        return;
    }

    const NacpStruct& nacp = ctrl->nacp;
    uint64_t owner = nacp.save_data_owner_id;
    g_lastStartupUserAccount = nacp.startup_user_account;
    g_lastStartupUserAccountOption = nacp.startup_user_account_option;
    g_lastLaunchAcceptsUser = g_lastStartupUserAccount != 0;
    g_lastLaunchNeedsUser = startupUserRequiresInteractiveSelection(g_lastStartupUserAccount,
                                                                    g_lastStartupUserAccountOption);

    switchu::FileLog::log("[app] NACP: startup_user=%u startup_user_option=%u accepts_user=%d needs_user=%d switch_lock=%u user_save=0x%lX device_save=0x%lX cache=0x%lX bcat=0x%lX",
                          (unsigned)nacp.startup_user_account,
                          (unsigned)nacp.startup_user_account_option,
                          g_lastLaunchAcceptsUser ? 1 : 0,
                          g_lastLaunchNeedsUser ? 1 : 0,
                          (unsigned)nacp.user_account_switch_lock,
                          nacp.user_account_save_data_size,
                          nacp.device_save_data_size,
                          nacp.cache_storage_size,
                          nacp.bcat_delivery_cache_storage_size);

    ensureSaveData(title_id, owner, uid,
                   FsSaveDataType_Account, FsSaveDataSpaceId_User,
                   nacp.user_account_save_data_size,
                   nacp.user_account_save_data_journal_size);

    AccountUid emptyUid = {};
    ensureSaveData(title_id, owner, emptyUid,
                   FsSaveDataType_Device, FsSaveDataSpaceId_User,
                   nacp.device_save_data_size,
                   nacp.device_save_data_journal_size);

    ensureSaveData(title_id, owner, emptyUid,
                   FsSaveDataType_Temporary, FsSaveDataSpaceId_Temporary,
                   nacp.temporary_storage_size, 0);

    ensureSaveData(title_id, owner, emptyUid,
                   FsSaveDataType_Cache, FsSaveDataSpaceId_User,
                   nacp.cache_storage_size,
                   nacp.cache_storage_journal_size);

    ensureSaveData(title_id, 0x010000000000000C, emptyUid,
                   FsSaveDataType_Bcat, FsSaveDataSpaceId_User,
                   nacp.bcat_delivery_cache_storage_size, 0x200000);

    delete ctrl;
    switchu::FileLog::log("[app] save data ensured for 0x%016lX", title_id);
}

inline Result launch(uint64_t title_id, AccountUid uid) {
    switchu::FileLog::log("[app] launch request title=0x%016lX running=%d fg=%d suspended=0x%016lX uid_valid=%d uid[0]=0x%016lX uid[1]=0x%016lX",
                          title_id,
                          g_running ? 1 : 0,
                          g_hasForeground ? 1 : 0,
                          g_suspendedTitleId,
                          accountUidIsValid(&uid) ? 1 : 0,
                          uid.uid[0], uid.uid[1]);
    if (g_running) {
        switchu::FileLog::log("[app] closing previous app before launch");
        appletApplicationRequestExit(&g_app);
        appletApplicationJoin(&g_app);
        appletApplicationClose(&g_app);
        g_running = false;
    }
    appletApplicationClose(&g_app);

    // nsTouchApplication prepares the title in the NS service (same as ulaunch/qlaunch).
    // Non-fatal: some special titles (stubs, forwarders) may return an error here.
    Result touchRc = nsTouchApplication(title_id);
    if (R_FAILED(touchRc))
        switchu::FileLog::log("[app] nsTouchApplication FAIL: 0x%X (non-fatal)", touchRc);
    else
        switchu::FileLog::log("[app] nsTouchApplication ok");

    if (kEnableSaveDataEnsure) {
        g_lastLaunchAcceptsUser = true;
        g_lastLaunchNeedsUser = true;
        g_lastStartupUserAccount = 1;
        g_lastStartupUserAccountOption = 0;
        ensureApplicationSaveData(title_id, uid);
    } else {
        g_lastLaunchAcceptsUser = true;
        g_lastLaunchNeedsUser = true;
        g_lastStartupUserAccount = 1;
        g_lastStartupUserAccountOption = 0;
        switchu::FileLog::log("[app] save data precreate skipped for 0x%016lX", title_id);
    }

    Result rc = appletCreateApplication(&g_app, title_id);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[app] CreateApp FAIL: 0x%X", rc);
        return rc;
    }

    struct {
        u32 magic;
        u8  is_selected;
        u8  pad[3];
        AccountUid uid;
        u8  unused[0x70];
    } userArg = {};
    static_assert(sizeof(userArg) == 0x88);

    if (g_lastLaunchAcceptsUser && accountUidIsValid(&uid)) {
        switchu::FileLog::log("[app] preselecting user startup_user=%u option=%u needs_user=%d uid[0]=0x%016lX uid[1]=0x%016lX",
                              (unsigned)g_lastStartupUserAccount,
                              (unsigned)g_lastStartupUserAccountOption,
                              g_lastLaunchNeedsUser ? 1 : 0,
                              uid.uid[0], uid.uid[1]);
        userArg.magic       = 0xC79497CA;
        userArg.is_selected = 1;
        userArg.uid         = uid;

        AppletStorage st;
        rc = appletCreateStorage(&st, sizeof(userArg));
        if (R_SUCCEEDED(rc)) {
            Result writeRc = appletStorageWrite(&st, 0, &userArg, sizeof(userArg));
            if (R_SUCCEEDED(writeRc)) {
                Result pushRc = appletApplicationPushLaunchParameter(&g_app,
                    AppletLaunchParameterKind_PreselectedUser, &st);
                if (R_FAILED(pushRc)) {
                    switchu::FileLog::log("[app] PushUser FAIL: 0x%X", pushRc);
                }
            } else {
                switchu::FileLog::log("[app] PushUser storage write FAIL: 0x%X", writeRc);
            }
            appletStorageClose(&st);
        } else {
            switchu::FileLog::log("[app] PushUser storage create FAIL: 0x%X", rc);
        }
    } else {
        switchu::FileLog::log("[app] launch without preselected user (accepts_user=%d needs_user=%d startup_user=%u option=%u uid_valid=%d)",
                              g_lastLaunchAcceptsUser ? 1 : 0,
                              g_lastLaunchNeedsUser ? 1 : 0,
                              (unsigned)g_lastStartupUserAccount,
                              (unsigned)g_lastStartupUserAccountOption,
                              accountUidIsValid(&uid) ? 1 : 0);
    }

    appletUnlockForeground();

    switchu::FileLog::log("[app] Start call");
    rc = appletApplicationStart(&g_app);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[app] Start FAIL: 0x%X", rc);
        appletApplicationClose(&g_app);
        return rc;
    }
    switchu::FileLog::log("[app] Start ok");

    rc = appletApplicationRequestForApplicationToGetForeground(&g_app);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[app] ReqFG FAIL: 0x%X", rc);
        appletApplicationClose(&g_app);
        return rc;
    }
    switchu::FileLog::log("[app] ReqFG ok");

    g_running = true;
    g_hasForeground = true;
    g_suspendedTitleId = title_id;
    switchu::FileLog::log("[app] launched 0x%016lX", title_id);
    return 0;
}

inline Result resume() {
    if (!g_running) return MAKERESULT(Module_Libnx, 0xFE);
    switchu::FileLog::log("[app] resume request fg=%d suspended=0x%016lX",
                          g_hasForeground ? 1 : 0, g_suspendedTitleId);
    appletUnlockForeground();
    Result rc = appletApplicationRequestForApplicationToGetForeground(&g_app);
    if (R_FAILED(rc))
        switchu::FileLog::log("[app] resume ReqFG FAIL: 0x%X", rc);
    else
        switchu::FileLog::log("[app] resume ReqFG ok");
    g_hasForeground = true;
    return rc;
}

inline Result areLibraryAppletsLeft(bool* out) {
    if (!out) return MAKERESULT(Module_Libnx, 0xFD);
    *out = false;
    if (!g_running) return 0;
    Result rc = appletApplicationAreAnyLibraryAppletsLeft(&g_app, out);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[app] AreAnyLibraryAppletsLeft FAIL: 0x%X", rc);
    } else {
        switchu::FileLog::log("[app] AreAnyLibraryAppletsLeft -> %d", *out ? 1 : 0);
    }
    return rc;
}

inline Result requestExitLibraryAppletOrTerminate(u64 timeout) {
    if (!g_running) return 0;
    switchu::FileLog::log("[app] RequestExitLibraryAppletOrTerminate timeout=%lu app=0x%016lX",
                          timeout, g_suspendedTitleId);
    Result rc = appletApplicationRequestExitLibraryAppletOrTerminate(&g_app, timeout);
    if (R_FAILED(rc))
        switchu::FileLog::log("[app] RequestExitLibraryAppletOrTerminate FAIL: 0x%X", rc);
    else
        switchu::FileLog::log("[app] RequestExitLibraryAppletOrTerminate ok");
    return rc;
}

inline Result terminate() {
    if (!g_running) return 0;
    switchu::FileLog::log("[app] terminate request app=0x%016lX fg=%d",
                          g_suspendedTitleId, g_hasForeground ? 1 : 0);
    Result libRc = appletApplicationTerminateAllLibraryApplets(&g_app);
    switchu::FileLog::log("[app] terminate TerminateAllLibraryApplets rc=0x%X", libRc);
    Result requestRc = appletApplicationRequestExit(&g_app);
    switchu::FileLog::log("[app] terminate RequestExit rc=0x%X", requestRc);
    Result waitRc = eventWait(&g_app.StateChangedEvent, 15'000'000'000ULL);
    if (waitRc == KERNELRESULT(TimedOut)) {
        switchu::FileLog::log("[app] terminate graceful wait timed out; forcing terminate");
        Result forceRc = appletApplicationTerminate(&g_app);
        switchu::FileLog::log("[app] terminate force rc=0x%X", forceRc);
    } else {
        switchu::FileLog::log("[app] terminate wait rc=0x%X", waitRc);
    }
    Result resultRc = serviceDispatch(&g_app.s, 30);
    switchu::FileLog::log("[app] terminate result rc=0x%X", resultRc);
    appletApplicationClose(&g_app);
    g_running = false;
    g_hasForeground = false;
    g_suspendedTitleId = 0;
    return 0;
}

inline bool checkFinished() {
    if (!g_running) return false;
    if (appletApplicationCheckFinished(&g_app)) {
        switchu::FileLog::log("[app] finished (reason=%d)",
            (int)appletApplicationGetExitReason(&g_app));
        appletApplicationJoin(&g_app);
        appletApplicationClose(&g_app);
        g_running = false;
        g_hasForeground = false;
        g_suspendedTitleId = 0;
        return true;
    }
    return false;
}

inline void onHomeSuspend() {
    switchu::FileLog::log("[app] onHomeSuspend fg %d -> 0 app=0x%016lX",
                          g_hasForeground ? 1 : 0, g_suspendedTitleId);
    g_hasForeground = false;
}

inline void cleanup() {
    if (g_running) {
        appletApplicationRequestExit(&g_app);
        appletApplicationJoin(&g_app);
        appletApplicationClose(&g_app);
        g_running = false;
    }
}

}
