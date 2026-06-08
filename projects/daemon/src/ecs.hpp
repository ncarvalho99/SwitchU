#pragma once
#include <switch.h>
#include <cstdint>

namespace switchu::daemon {

void initializeExternalContentAllocator();
Result registerExternalContent(uint64_t program_id, const char* exefs_path);
void unregisterExternalContent(uint64_t program_id);

}
