#pragma once
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <switchu/log_utils.hpp>

namespace switchu {

class FileLog {
public:
    static constexpr const char* LOG_DIR = "sdmc:/config/SwitchU";
    static constexpr const char* LOG_EXTENSION = ".log";
    static constexpr size_t MAX_LOG_FILES = 5;
    static constexpr size_t MAX_ARCHIVED_LOGS = MAX_LOG_FILES - 1;

    static void open(const char* tag) {
        auto& self = inst();
        std::lock_guard<std::mutex> lock(self.m_mutex);

        log_detail::ensure_log_dir(LOG_DIR);
        close_current_file(self);

        const bool can_truncate = log_detail::rotate_current_log(LOG_DIR, tag, LOG_EXTENSION, MAX_ARCHIVED_LOGS);

        char path[256];
        log_detail::build_current_log_path(path, sizeof(path), LOG_DIR, tag, LOG_EXTENSION);
        self.m_file = std::fopen(path, can_truncate ? "w" : "a");
        if (self.m_file) {
            std::setvbuf(self.m_file, nullptr, _IOLBF, 0);
            char timestamp[32];
            log_detail::format_line_timestamp(timestamp, sizeof(timestamp));
            std::fprintf(self.m_file, "[%s] === %s log start ===\n", timestamp, tag);
            std::fflush(self.m_file);
        }
    }

    static void close() {
        auto& self = inst();
        std::lock_guard<std::mutex> lock(self.m_mutex);
        close_current_file(self);
    }

    static void log(const char* fmt, ...) {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        char timestamp[32];
        log_detail::format_line_timestamp(timestamp, sizeof(timestamp));

        auto& self = inst();
        std::lock_guard<std::mutex> lock(self.m_mutex);

        std::fprintf(stderr, "[%s] %s\n", timestamp, buf);
        if (self.m_file) {
            std::fprintf(self.m_file, "[%s] %s\n", timestamp, buf);
            std::fflush(self.m_file);
        }
    }

private:
    static FileLog& inst() { static FileLog s; return s; }

    static void close_current_file(FileLog& self) {
        if (!self.m_file)
            return;

        char timestamp[32];
        log_detail::format_line_timestamp(timestamp, sizeof(timestamp));
        std::fprintf(self.m_file, "[%s] === log end ===\n", timestamp);
        std::fclose(self.m_file);
        self.m_file = nullptr;
    }

    std::mutex m_mutex;
    FILE* m_file = nullptr;
};

}
