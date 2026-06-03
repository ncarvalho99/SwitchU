#pragma once
#include <switchu/smi_protocol.hpp>
#include <switchu/file_log.hpp>
#include <switch.h>
#include <cstdio>
#include <cstring>
#include <atomic>

namespace switchu::daemon {

namespace {
static constexpr size_t kExternalFsProxyStackSize = 0x8000;

struct ExternalFsProxy {
    Handle server = INVALID_HANDLE;
    FsFileSystem fs = {};
    Thread thread = {};
    alignas(0x1000) uint8_t stack[kExternalFsProxyStackSize] = {};
    std::atomic<bool> active{false};
};

static ExternalFsProxy g_externalFsProxy;

static void externalFsProxyThread(void*) {
    switchu::FileLog::log("[ecs] proxy thread started server=0x%X fs=0x%X",
                          g_externalFsProxy.server,
                          g_externalFsProxy.fs.s.session);

    Handle replyTarget = INVALID_HANDLE;
    Handle handles[1] = { g_externalFsProxy.server };

    while (g_externalFsProxy.active.load()) {
        s32 index = -1;
        Result rc = svcReplyAndReceive(&index, handles, 1, replyTarget, UINT64_MAX);
        replyTarget = INVALID_HANDLE;

        if (R_FAILED(rc)) {
            switchu::FileLog::log("[ecs] proxy ReplyAndReceive FAIL: 0x%X", rc);
            break;
        }

        if (index != 0)
            continue;

        rc = svcSendSyncRequest(g_externalFsProxy.fs.s.session);
        if (R_FAILED(rc)) {
            switchu::FileLog::log("[ecs] proxy forward FAIL: 0x%X", rc);
            break;
        }

        replyTarget = g_externalFsProxy.server;
    }

    if (replyTarget != INVALID_HANDLE) {
        s32 index = -1;
        svcReplyAndReceive(&index, handles, 0, replyTarget, 0);
    }

    fsFsClose(&g_externalFsProxy.fs);
    if (g_externalFsProxy.server != INVALID_HANDLE)
        svcCloseHandle(g_externalFsProxy.server);
    g_externalFsProxy.server = INVALID_HANDLE;
    g_externalFsProxy.active.store(false);
    switchu::FileLog::log("[ecs] proxy thread exiting");
}
}

inline Result ldrAtmosRegisterExternalCode(uint64_t program_id, Handle* out_h) {
    return serviceDispatchIn(ldrShellGetServiceSession(),
        smi::kLdrAtmosRegisterExternalCode, program_id,
        .out_handle_attrs = { SfOutHandleAttr_HipcMove },
        .out_handles = out_h,
    );
}

inline Result ldrAtmosUnregisterExternalCode(uint64_t program_id) {
    return serviceDispatchIn(ldrShellGetServiceSession(),
        smi::kLdrAtmosUnregisterExternalCode, program_id);
}


static Result fspSrvOpenSubDirectoryFileSystem(FsFileSystem* baseFs, FsFileSystem* out, const char* sub_path) {
    char pathBuf[FS_MAX_PATH] = {};
    std::strncpy(pathBuf, sub_path, FS_MAX_PATH - 1);
    return serviceDispatch(&baseFs->s, 18,
        .buffer_attrs = { SfBufferAttr_HipcPointer | SfBufferAttr_In | SfBufferAttr_FixedSize },
        .buffers = { { pathBuf, FS_MAX_PATH } },
        .out_num_objects = 1,
        .out_objects = &out->s,
    );
}

inline Result registerExternalContent(uint64_t program_id, const char* exefs_path) {
    if (g_externalFsProxy.active.load()) {
        switchu::FileLog::log("[ecs] proxy already active; leaving previous session alive");
        return 0;
    }

    Handle move_h = INVALID_HANDLE;
    Result rc = ldrAtmosRegisterExternalCode(program_id, &move_h);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[ecs] RegisterExternalCode FAIL: 0x%X", rc);
        return rc;
    }
    switchu::FileLog::log("[ecs] RegisterExternalCode ok program=0x%016lX server=0x%X",
                          program_id, move_h);

    FsFileSystem sd_fs;
    rc = fsOpenSdCardFileSystem(&sd_fs);
    if (R_FAILED(rc)) {
        svcCloseHandle(move_h);
        switchu::FileLog::log("[ecs] fsOpenSdCard FAIL: 0x%X", rc);
        return rc;
    }

    FsFileSystem sub_fs;
    rc = fspSrvOpenSubDirectoryFileSystem(&sd_fs, &sub_fs, exefs_path);
    fsFsClose(&sd_fs);

    if (R_FAILED(rc)) {
        svcCloseHandle(move_h);
        switchu::FileLog::log("[ecs] OpenSubDir FAIL: 0x%X path=%s", rc, exefs_path);
        return rc;
    }

    g_externalFsProxy.server = move_h;
    g_externalFsProxy.fs = sub_fs;
    g_externalFsProxy.active.store(true);
    rc = threadCreate(&g_externalFsProxy.thread, externalFsProxyThread, nullptr,
                      g_externalFsProxy.stack, sizeof(g_externalFsProxy.stack),
                      18, -2);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[ecs] proxy threadCreate FAIL: 0x%X", rc);
        g_externalFsProxy.active.store(false);
        fsFsClose(&g_externalFsProxy.fs);
        svcCloseHandle(move_h);
        g_externalFsProxy.server = INVALID_HANDLE;
        return rc;
    }

    rc = threadStart(&g_externalFsProxy.thread);
    if (R_FAILED(rc)) {
        switchu::FileLog::log("[ecs] proxy threadStart FAIL: 0x%X", rc);
        threadClose(&g_externalFsProxy.thread);
        g_externalFsProxy.active.store(false);
        fsFsClose(&g_externalFsProxy.fs);
        svcCloseHandle(move_h);
        g_externalFsProxy.server = INVALID_HANDLE;
        return rc;
    }

    switchu::FileLog::log("[ecs] registered %s -> 0x%016lX", exefs_path, program_id);

    return 0;
}

inline void unregisterExternalContent(uint64_t program_id) {
    Result rc = ldrAtmosUnregisterExternalCode(program_id);
    if (R_FAILED(rc))
        switchu::FileLog::log("[ecs] Unregister FAIL: 0x%X", rc);
    else
        switchu::FileLog::log("[ecs] unregistered 0x%016lX", program_id);
}

}
