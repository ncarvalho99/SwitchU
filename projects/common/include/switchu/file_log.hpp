#pragma once
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <string>
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
        self.m_sink.open(path, can_truncate);
        if (self.m_sink.is_open()) {
            char timestamp[32];
            log_detail::format_line_timestamp(timestamp, sizeof(timestamp));
            self.m_sink.append(std::string("[") + timestamp + "] === " + tag + " log start ===");
            self.m_sink.flush();
        }
    }

    static void close() {
        auto& self = inst();
        std::lock_guard<std::mutex> lock(self.m_mutex);
        close_current_file(self);
    }

    // Forces buffered lines out to the SD card. Call before anything that may
    // terminate the process without unwinding.
    static void flush() {
        auto& self = inst();
        std::lock_guard<std::mutex> lock(self.m_mutex);
        self.m_sink.flush();
    }

    // Drains lines that have been buffered for a while. Intended for processes
    // that run indefinitely and never reach close().
    static void flushIfStale(int maxAgeSeconds = 2) {
        auto& self = inst();
        std::lock_guard<std::mutex> lock(self.m_mutex);
        self.m_sink.flush_if_stale(maxAgeSeconds);
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
        self.m_sink.append(std::string("[") + timestamp + "] " + buf);
    }

private:
    static FileLog& inst() { static FileLog s; return s; }

    static void close_current_file(FileLog& self) {
        if (!self.m_sink.is_open())
            return;

        char timestamp[32];
        log_detail::format_line_timestamp(timestamp, sizeof(timestamp));
        self.m_sink.append(std::string("[") + timestamp + "] === log end ===");
        self.m_sink.close();
    }

    std::mutex m_mutex;
    log_detail::buffered_sink m_sink;
};

}
