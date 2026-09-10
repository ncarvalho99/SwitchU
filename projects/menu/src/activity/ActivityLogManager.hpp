#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace switchu::activity {

struct TitlePlayStats {
    std::uint64_t titleId = 0;
    std::string titleName;
    std::uint64_t totalPlaytimeSeconds = 0;
    std::uint32_t totalLaunches = 0;
    std::uint64_t firstPlayedTimestamp = 0; // POSIX seconds
    std::uint64_t lastPlayedTimestamp = 0;  // POSIX seconds
    std::uint64_t averageSessionSeconds = 0;
};

struct DailyTitleEntry {
    std::uint64_t titleId = 0;
    std::string titleName;
    std::uint64_t playtimeSeconds = 0;
    std::uint32_t launches = 0;
};

struct DailyLogDay {
    int year = 0;
    int month = 0; // 1-12
    int day = 0;   // 1-31
    std::uint64_t totalPlaytimeSeconds = 0;
    std::vector<DailyTitleEntry> titles;
};

struct MonthlyLogSummary {
    int year = 0;
    int month = 0; // 1-12
    std::uint64_t totalPlaytimeSeconds = 0;
    int activeDaysCount = 0;
    std::vector<std::uint64_t> dailyPlaytimeSeconds; // 31 entries (indices 0..30)
    std::vector<DailyTitleEntry> titles;             // sorted descending by playtime
};

class ActivityLogManager {
public:
    ActivityLogManager() = default;
    ~ActivityLogManager() = default;

    void refresh(const std::vector<std::pair<std::uint64_t, std::string>>& installedTitles);

    const std::vector<TitlePlayStats>& allTimeRankings() const { return m_allTimeRankings; }
    const TitlePlayStats* findTitleStats(std::uint64_t titleId) const;

    DailyLogDay queryDay(int year, int month, int day) const;
    MonthlyLogSummary queryMonth(int year, int month) const;

    bool hasData() const { return !m_allTimeRankings.empty() || !m_dailyRecords.empty(); }

    static int daysInMonth(int year, int month);

private:
    void queryPdmStatistics(const std::vector<std::pair<std::uint64_t, std::string>>& installedTitles);
    void queryPdmAppletEvents(const std::unordered_map<std::uint64_t, std::string>& titleNames);

    static int makeDayKey(int year, int month, int day) {
        return year * 10000 + month * 100 + day;
    }

    std::vector<TitlePlayStats> m_allTimeRankings;
    std::unordered_map<std::uint64_t, TitlePlayStats> m_statsByTitle;
    std::unordered_map<int, DailyLogDay> m_dailyRecords;
};

} // namespace switchu::activity
