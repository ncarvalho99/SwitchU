#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <switchu/log_utils.hpp>
#ifdef SWITCHU_MENU
#include <switchu/file_log.hpp>
#endif

class DebugLog {
public:
    static constexpr int MAX_LINES = 30;
    static constexpr const char* LOG_DIR  = "sdmc:/config/SwitchU";
    static constexpr const char* LOG_BASE_NAME = "log";
    static constexpr const char* LOG_EXTENSION = ".txt";
#ifdef SWITCHU_MENU
    static constexpr const char* LOG_FILE = "sdmc:/config/SwitchU/menu.log";
#else
    static constexpr const char* LOG_FILE = "sdmc:/config/SwitchU/log.txt";
#endif
    static constexpr size_t MAX_LOG_FILES = 5;
    static constexpr size_t MAX_ARCHIVED_LOGS = MAX_LOG_FILES - 1;

    static void openFileLog() {
        auto& self = instance();
        std::lock_guard<std::mutex> lk(self.m_mutex);

#ifdef SWITCHU_MENU
        closeCurrentFile(self);
        return;
#else
        switchu::log_detail::ensure_log_dir(LOG_DIR);
        closeCurrentFile(self);

        const bool canTruncate = switchu::log_detail::rotate_current_log(LOG_DIR, LOG_BASE_NAME, LOG_EXTENSION, MAX_ARCHIVED_LOGS);
        char path[256];
        switchu::log_detail::build_current_log_path(path, sizeof(path), LOG_DIR, LOG_BASE_NAME, LOG_EXTENSION);
        self.m_file = std::fopen(path, canTruncate ? "w" : "a");
        if (self.m_file) {
            std::setvbuf(self.m_file, nullptr, _IOLBF, 0);
            char timestamp[32];
            switchu::log_detail::format_line_timestamp(timestamp, sizeof(timestamp));
            std::fprintf(self.m_file, "[%s] === SwitchU log start ===\n", timestamp);
            std::fflush(self.m_file);
        }
#endif
    }

    static void closeFileLog() {
        auto& self = instance();
        std::lock_guard<std::mutex> lk(self.m_mutex);
        closeCurrentFile(self);
    }

    static void log(const char* fmt, ...) {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        char timestamp[32];
        switchu::log_detail::format_line_timestamp(timestamp, sizeof(timestamp));
        std::string line = std::string("[") + timestamp + "] " + buf;

        auto& self = instance();
        std::lock_guard<std::mutex> lk(self.m_mutex);

        self.m_lines.push_back(line);
        if ((int)self.m_lines.size() > MAX_LINES)
            self.m_lines.erase(self.m_lines.begin());

#ifdef SWITCHU_MENU
        switchu::FileLog::log("%s", buf);
#else
        if (self.m_file) {
            std::fprintf(self.m_file, "%s\n", line.c_str());
            std::fflush(self.m_file);
        }

        std::fprintf(stderr, "%s\n", line.c_str());
#endif
    }

    static std::vector<std::string> lines() {
        auto& self = instance();
        std::lock_guard<std::mutex> lk(self.m_mutex);
        return self.m_lines;
    }

private:
    static DebugLog& instance() {
        static DebugLog s;
        return s;
    }

    static void closeCurrentFile(DebugLog& self) {
        if (!self.m_file)
            return;

        char timestamp[32];
        switchu::log_detail::format_line_timestamp(timestamp, sizeof(timestamp));
        std::fprintf(self.m_file, "[%s] === SwitchU log end ===\n", timestamp);
        std::fclose(self.m_file);
        self.m_file = nullptr;
    }

    std::mutex m_mutex;
    std::vector<std::string> m_lines;
    FILE* m_file = nullptr;
};
