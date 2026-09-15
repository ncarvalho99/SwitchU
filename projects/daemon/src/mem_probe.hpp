#pragma once

#include <switch.h>
#include <switchu/file_log.hpp>
#include <cstdio>
#include <malloc.h>

namespace switchu::daemon::mem {

// Horizon splits DRAM into fixed pools. A game allocates from Application, the
// menu runs as a library applet out of Applet, and System is shared by every
// sysmodule on the card -- ours and everyone else's -- with the services a game
// needs on its way up. "Launching a game on a loaded console can still be
// unstable" is a claim about those numbers, and nothing was measuring them: the
// menu only ever logged its own process totals, and by launch time it is gone.
//
// Cheap enough to call at the handful of moments that matter: four pairs of
// svcGetSystemInfo plus two svcGetInfo, no allocation, one log line. Both
// syscalls are already declared in daemon.json (0x6F and 0x29).
inline void snapshot(const char* phase) {
    struct Pool {
        const char* name;
        u64 region;
    };
    static constexpr Pool kPools[] = {
        {"app",    PhysicalMemorySystemInfo_Application},
        {"applet", PhysicalMemorySystemInfo_Applet},
        {"system", PhysicalMemorySystemInfo_System},
        {"unsafe", PhysicalMemorySystemInfo_SystemUnsafe},
    };

    char line[384];
    int at = std::snprintf(line, sizeof(line), "[mem] %s", phase);
    for (const auto& pool : kPools) {
        u64 total = 0;
        u64 used = 0;
        const Result totalRc = svcGetSystemInfo(&total, SystemInfoType_TotalPhysicalMemorySize,
                                                INVALID_HANDLE, pool.region);
        const Result usedRc = svcGetSystemInfo(&used, SystemInfoType_UsedPhysicalMemorySize,
                                               INVALID_HANDLE, pool.region);
        if (at < 0 || at >= static_cast<int>(sizeof(line)))
            break;
        if (R_FAILED(totalRc) || R_FAILED(usedRc)) {
            at += std::snprintf(line + at, sizeof(line) - at, " %s=?", pool.name);
            continue;
        }
        // Free is the number that decides whether the next allocation fails,
        // so it is written out rather than left to be worked out by hand.
        at += std::snprintf(line + at, sizeof(line) - at, " %s=%lu/%luMB(free=%lu)",
                            pool.name,
                            static_cast<unsigned long>(used >> 20),
                            static_cast<unsigned long>(total >> 20),
                            static_cast<unsigned long>((total - used) >> 20));
    }

    u64 procUsed = 0;
    u64 procTotal = 0;
    if (R_SUCCEEDED(svcGetInfo(&procUsed, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0))
        && R_SUCCEEDED(svcGetInfo(&procTotal, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0))
        && at > 0 && at < static_cast<int>(sizeof(line))) {
        // What the process holds is what it was given, not what it needs: the
        // heap is mapped up front, so this figure never moves. malloc's own
        // accounting is what says whether the heap can be made smaller -- and
        // on a console whose System pool has ten megabytes free, this daemon's
        // twelve are worth arguing about.
        const struct mallinfo info = mallinfo();
        at += std::snprintf(line + at, sizeof(line) - at,
                            " daemon=%luKB/%luKB heap=%luKB",
                            static_cast<unsigned long>(procUsed >> 10),
                            static_cast<unsigned long>(procTotal >> 10),
                            static_cast<unsigned long>(info.uordblks >> 10));
    }

    switchu::FileLog::log("%s", line);
}

} // namespace switchu::daemon::mem
