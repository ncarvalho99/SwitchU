#include "ActivityLogManager.hpp"
#include "core/DebugLog.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <ctime>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace switchu::activity {

namespace {

inline std::uint64_t toPosixTimestamp(std::uint64_t ts) {
    if (ts == 0) return 0;
    if (ts > 1000000000ULL) return ts;
    return ts * 60 + 946598400ULL;
}

void timestampToDate(std::uint64_t posixSeconds, int& outYear, int& outMonth, int& outDay) {
    if (posixSeconds == 0) {
        outYear = 2026;
        outMonth = 1;
        outDay = 1;
        return;
    }
    std::time_t raw = static_cast<std::time_t>(posixSeconds);
    std::tm t{};
#if defined(_WIN32) && !defined(__SWITCH__)
    localtime_s(&t, &raw);
#else
    localtime_r(&raw, &t);
#endif
    outYear = t.tm_year + 1900;
    outMonth = t.tm_mon + 1;
    outDay = t.tm_mday;
}

} // namespace

int ActivityLogManager::daysInMonth(int year, int month) {
    if (month == 2) {
        const bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

void ActivityLogManager::refresh(const std::vector<std::pair<std::uint64_t, std::string>>& installedTitles) {
    m_allTimeRankings.clear();
    m_statsByTitle.clear();
    m_dailyRecords.clear();

    std::unordered_map<std::uint64_t, std::string> titleNames;
    for (const auto& [tid, name] : installedTitles) {
        if (tid != 0) {
            titleNames[tid] = name;
        }
    }

    queryPdmStatistics(installedTitles);
    queryPdmAppletEvents(titleNames);

    // If event logs are sparse or empty, ensure any title with non-zero playtime has at least
    // a representation on its last-played date so calendar navigation is populated.
    for (const auto& stats : m_allTimeRankings) {
        if (stats.totalPlaytimeSeconds > 0 && stats.lastPlayedTimestamp > 0) {
            int y = 0, m = 0, d = 0;
            timestampToDate(stats.lastPlayedTimestamp, y, m, d);
            int dayKey = makeDayKey(y, m, d);
            auto& dayRecord = m_dailyRecords[dayKey];
            dayRecord.year = y;
            dayRecord.month = m;
            dayRecord.day = d;

            auto it = std::find_if(dayRecord.titles.begin(), dayRecord.titles.end(),
                                   [tid = stats.titleId](const DailyTitleEntry& e) {
                                       return e.titleId == tid;
                                   });
            if (it == dayRecord.titles.end()) {
                DailyTitleEntry entry;
                entry.titleId = stats.titleId;
                entry.titleName = stats.titleName;
                // Estimate day playtime as min of average session or 1 hour
                entry.playtimeSeconds = std::max<std::uint64_t>(
                    60, std::min<std::uint64_t>(stats.averageSessionSeconds > 0 ? stats.averageSessionSeconds : 1800, 3600 * 2));
                entry.launches = std::max<std::uint32_t>(1, stats.totalLaunches > 0 ? 1 : 0);
                dayRecord.titles.push_back(entry);
                dayRecord.totalPlaytimeSeconds += entry.playtimeSeconds;
            }
        }
    }

    // Sort daily titles descending by playtime
    for (auto& [_, day] : m_dailyRecords) {
        std::sort(day.titles.begin(), day.titles.end(),
                  [](const DailyTitleEntry& a, const DailyTitleEntry& b) {
                      return a.playtimeSeconds > b.playtimeSeconds;
                  });
    }

    DebugLog::log("[activity] refresh complete: %zu ranked titles, %zu active days",
                  m_allTimeRankings.size(), m_dailyRecords.size());
}

const TitlePlayStats* ActivityLogManager::findTitleStats(std::uint64_t titleId) const {
    auto it = m_statsByTitle.find(titleId);
    if (it != m_statsByTitle.end())
        return &it->second;
    return nullptr;
}

DailyLogDay ActivityLogManager::queryDay(int year, int month, int day) const {
    int key = makeDayKey(year, month, day);
    auto it = m_dailyRecords.find(key);
    if (it != m_dailyRecords.end())
        return it->second;

    DailyLogDay empty;
    empty.year = year;
    empty.month = month;
    empty.day = day;
    return empty;
}

MonthlyLogSummary ActivityLogManager::queryMonth(int year, int month) const {
    MonthlyLogSummary summary;
    summary.year = year;
    summary.month = month;
    summary.dailyPlaytimeSeconds.resize(31, 0);

    std::unordered_map<std::uint64_t, DailyTitleEntry> titleMap;

    for (int day = 1; day <= 31; ++day) {
        int key = makeDayKey(year, month, day);
        auto it = m_dailyRecords.find(key);
        if (it != m_dailyRecords.end()) {
            summary.dailyPlaytimeSeconds[day - 1] = it->second.totalPlaytimeSeconds;
            summary.totalPlaytimeSeconds += it->second.totalPlaytimeSeconds;
            if (it->second.totalPlaytimeSeconds > 0)
                summary.activeDaysCount++;

            for (const auto& t : it->second.titles) {
                auto& agg = titleMap[t.titleId];
                agg.titleId = t.titleId;
                agg.titleName = t.titleName;
                agg.playtimeSeconds += t.playtimeSeconds;
                agg.launches += t.launches;
            }
        }
    }

    for (auto& [_, t] : titleMap) {
        summary.titles.push_back(t);
    }
    std::sort(summary.titles.begin(), summary.titles.end(),
              [](const DailyTitleEntry& a, const DailyTitleEntry& b) {
                  return a.playtimeSeconds > b.playtimeSeconds;
              });

    return summary;
}

void ActivityLogManager::queryPdmStatistics(const std::vector<std::pair<std::uint64_t, std::string>>& installedTitles) {
#ifdef __SWITCH__
    PdmPlayStatistics stats{};
    const Result initRc = pdmqryInitialize();
    if (R_FAILED(initRc)) {
        DebugLog::log("[activity] pdmqryInitialize failed rc=0x%08X", (unsigned int)initRc);
        return;
    }

    for (const auto& [titleId, name] : installedTitles) {
        if (titleId == 0) continue;
        std::memset(&stats, 0, sizeof(stats));
        const Result qrc = pdmqryQueryPlayStatisticsByApplicationId(titleId, true, &stats);
        if (R_SUCCEEDED(qrc)) {
            TitlePlayStats s;
            s.titleId = titleId;
            s.titleName = name;
            s.totalPlaytimeSeconds = stats.playtime / 1000000000ULL;
            s.totalLaunches = stats.total_launches;
            s.firstPlayedTimestamp = toPosixTimestamp(stats.first_timestamp_user);
            s.lastPlayedTimestamp = toPosixTimestamp(stats.last_timestamp_user);
            s.averageSessionSeconds = s.totalLaunches > 0 ? (s.totalPlaytimeSeconds / s.totalLaunches) : 0;

            m_statsByTitle[titleId] = s;
            m_allTimeRankings.push_back(s);
        }
    }
    pdmqryExit();
#else
    // Development/test mock entries
    for (size_t i = 0; i < installedTitles.size(); ++i) {
        const auto& [titleId, name] = installedTitles[i];
        if (titleId == 0) continue;
        TitlePlayStats s;
        s.titleId = titleId;
        s.titleName = name;
        s.totalPlaytimeSeconds = (installedTitles.size() - i) * 3600ULL * 4 + (i * 1234ULL % 3600);
        s.totalLaunches = static_cast<std::uint32_t>((installedTitles.size() - i) * 5 + 1);
        s.firstPlayedTimestamp = 1704067200ULL + i * 86400; // ~Jan 2024
        s.lastPlayedTimestamp = 1789000000ULL - i * 43200;
        s.averageSessionSeconds = s.totalLaunches > 0 ? (s.totalPlaytimeSeconds / s.totalLaunches) : 0;

        m_statsByTitle[titleId] = s;
        m_allTimeRankings.push_back(s);
    }
#endif

    std::sort(m_allTimeRankings.begin(), m_allTimeRankings.end(),
              [](const TitlePlayStats& a, const TitlePlayStats& b) {
                  if (a.totalPlaytimeSeconds != b.totalPlaytimeSeconds)
                      return a.totalPlaytimeSeconds > b.totalPlaytimeSeconds;
                  return a.totalLaunches > b.totalLaunches;
              });
}

void ActivityLogManager::queryPdmAppletEvents(const std::unordered_map<std::uint64_t, std::string>& titleNames) {
#ifdef __SWITCH__
    const Result initRc = pdmqryInitialize();
    if (R_FAILED(initRc)) return;

    s32 total_entries = 0, start_entry = 0, end_entry = 0;
    const Result rangeRc = pdmqryGetAvailablePlayEventRange(&total_entries, &start_entry, &end_entry);
    if (R_SUCCEEDED(rangeRc) && total_entries > 0 && end_entry >= start_entry) {
        constexpr s32 kChunkSize = 64;
        std::vector<PdmAppletEvent> events(kChunkSize);
        std::unordered_map<u64, u64> activeStarts;

        // Scan the most recent applet events to keep UI opening instantaneous.
        s32 cur = std::max(start_entry, end_entry - 120);
        while (cur <= end_entry) {
            s32 count = std::min(kChunkSize, end_entry - cur + 1);
            s32 total_out = 0;
            const Result eqRc = pdmqryQueryAppletEvent(cur, true, events.data(), count, &total_out);
            if (R_FAILED(eqRc) || total_out <= 0)
                break;

            for (s32 i = 0; i < total_out; ++i) {
                const auto& ev = events[i];
                if (ev.program_id == 0) continue;

                u64 ts = toPosixTimestamp(ev.timestamp_user);
                if (ev.event_type == PdmAppletEventType_Launch || ev.event_type == PdmAppletEventType_InFocus) {
                    activeStarts[ev.program_id] = ts;
                } else if (ev.event_type == PdmAppletEventType_Exit || ev.event_type == PdmAppletEventType_OutOfFocus ||
                           ev.event_type == PdmAppletEventType_OutOfFocus4 || ev.event_type == PdmAppletEventType_Exit5 ||
                           ev.event_type == PdmAppletEventType_Exit6) {
                    auto it = activeStarts.find(ev.program_id);
                    if (it != activeStarts.end()) {
                        u64 startTs = it->second;
                        activeStarts.erase(it);
                        if (ts >= startTs) {
                            u64 duration = ts - startTs;
                            if (duration > 0 && duration < 24 * 3600) {
                                int y = 0, m = 0, d = 0;
                                timestampToDate(startTs, y, m, d);
                                int dayKey = makeDayKey(y, m, d);

                                auto& dayRecord = m_dailyRecords[dayKey];
                                dayRecord.year = y;
                                dayRecord.month = m;
                                dayRecord.day = d;
                                dayRecord.totalPlaytimeSeconds += duration;

                                auto tit = std::find_if(dayRecord.titles.begin(), dayRecord.titles.end(),
                                                        [id = ev.program_id](const DailyTitleEntry& e) {
                                                            return e.titleId == id;
                                                        });
                                if (tit != dayRecord.titles.end()) {
                                    tit->playtimeSeconds += duration;
                                    tit->launches += 1;
                                } else {
                                    DailyTitleEntry te;
                                    te.titleId = ev.program_id;
                                    auto nit = titleNames.find(ev.program_id);
                                    te.titleName = (nit != titleNames.end()) ? nit->second : "";
                                    te.playtimeSeconds = duration;
                                    te.launches = 1;
                                    dayRecord.titles.push_back(te);
                                }
                            }
                        }
                    }
                }
            }
            cur += total_out;
        }
    }
    pdmqryExit();
#else
    (void)titleNames;
#endif
}

} // namespace switchu::activity
