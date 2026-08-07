#pragma once
#include <cstdarg>
#include <cstdio>
struct DebugLog {
    static void log(const char* fmt, ...) {
        std::va_list ap; va_start(ap, fmt);
        std::fputs("  [log] ", stdout); std::vprintf(fmt, ap); std::fputc('\n', stdout);
        va_end(ap);
    }
};
