#pragma once
#include <switch.h>
#include <cstdio>
#include <cstring>

namespace switchu {

// Flush the SD card's filesystem metadata.
//
// Writes made through the fsdev devoptab are not durable when they return.
// fsdevCommitDevice maps to fsFsCommit, and until that runs the FAT metadata
// for anything just written may still be dirty. Rebooting in that state can
// leave the card unmountable, which happened three times: hekate came up
// unable to mount the SD and the firmware files had to be restored.
//
// Call after writing anything worth keeping, and before any reboot, shutdown
// or sleep sequence. Cheap when there is nothing outstanding.
inline bool commitSdCard(const char* reason) {
    const Result rc = fsdevCommitDevice("sdmc");
    if (R_FAILED(rc)) {
        // Deliberately not routed through FileLog: the log itself lives on the
        // card being committed, and callers may already have closed it.
        char msg[96];
        std::snprintf(msg, sizeof(msg), "[sd] commit FAIL (%s) rc=0x%X", reason, rc);
        svcOutputDebugString(msg, std::strlen(msg));
        return false;
    }
    (void)reason;
    return true;
}

} // namespace switchu
