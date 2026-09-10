#include "ActivityLogScreen.hpp"
#include "core/DebugLog.hpp"

#include <nxui/core/Renderer.hpp>
#include <nxui/core/I18n.hpp>
#include <nxui/core/Input.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

constexpr float kHeaderH = 46.f;
constexpr float kSummaryH = 50.f;
constexpr float kItemGap = 8.f;
constexpr float kDailyItemH = 62.f;
constexpr float kTitleItemH = 68.f;

// Wii U inspired palette
const nxui::Color kColorCyan(0.00f, 0.65f, 0.88f, 1.0f);
const nxui::Color kColorDarkCyan(0.00f, 0.45f, 0.65f, 1.0f);
const nxui::Color kColorGold(1.00f, 0.82f, 0.10f, 1.0f);
const nxui::Color kColorSilver(0.80f, 0.83f, 0.88f, 1.0f);
const nxui::Color kColorBronze(0.82f, 0.52f, 0.25f, 1.0f);
const nxui::Color kCardBg(1.00f, 1.00f, 1.00f, 0.08f);
const nxui::Color kCardBgHighlight(1.00f, 1.00f, 1.00f, 0.16f);
const nxui::Color kBarTrack(0.00f, 0.00f, 0.00f, 0.35f);

const nxui::Color kTitleBarColors[] = {
    nxui::Color(0.00f, 0.68f, 0.92f, 1.0f), // Wii U Sky Blue
    nxui::Color(0.12f, 0.78f, 0.40f, 1.0f), // Emerald Green
    nxui::Color(1.00f, 0.55f, 0.05f, 1.0f), // Vivid Orange
    nxui::Color(0.68f, 0.25f, 0.95f, 1.0f), // Royal Violet
    nxui::Color(0.95f, 0.20f, 0.55f, 1.0f), // Magenta
    nxui::Color(1.00f, 0.82f, 0.10f, 1.0f), // Golden Sun
    nxui::Color(0.20f, 0.50f, 1.00f, 1.0f), // Cobalt
    nxui::Color(0.00f, 0.75f, 0.65f, 1.0f), // Teal
};

const char* const kMonthNamesEn[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

const char* const kDayNamesEn[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

int dayOfWeek(int y, int m, int d) {
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

std::string ellipsizeText(nxui::Font* font, const std::string& text, float maxW, float scale) {
    if (!font || text.empty() || font->measure(text).x * scale <= maxW)
        return text;
    std::string res = text;
    while (!res.empty() && font->measure(res + "...").x * scale > maxW) {
        res.pop_back();
    }
    return res.empty() ? "..." : res + "...";
}

} // namespace

ActivityLogScreen::ActivityLogScreen()
    : TabbedOverlayScreen(ScreenMode::ActivityLog) {
    addAction(static_cast<uint64_t>(nxui::Button::L),  [this]() { cycleTab(-1); });
    addAction(static_cast<uint64_t>(nxui::Button::R),  [this]() { cycleTab(1); });
    addAction(static_cast<uint64_t>(nxui::Button::ZL), [this]() {
        if (m_tabIndex == 0) cycleDay(-1);
        else if (m_tabIndex == 1) cycleMonth(-1);
    });
    addAction(static_cast<uint64_t>(nxui::Button::ZR), [this]() {
        if (m_tabIndex == 0) cycleDay(1);
        else if (m_tabIndex == 1) cycleMonth(1);
    });
    addAction(static_cast<uint64_t>(nxui::Button::Y),  [this]() { jumpToToday(); });
}

ActivityLogScreen::~ActivityLogScreen() = default;

void ActivityLogScreen::open(std::uint64_t initialTitleId) {
    std::time_t now = std::time(nullptr);
    std::tm t{};
#if defined(_WIN32) && !defined(__SWITCH__)
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    m_todayYear  = t.tm_year + 1900;
    m_todayMonth = t.tm_mon + 1;
    m_todayDay   = t.tm_mday;

    m_curYear  = m_todayYear;
    m_curMonth = m_todayMonth;
    m_curDay   = m_todayDay;

    m_monthYear  = m_todayYear;
    m_monthMonth = m_todayMonth;
    m_monthlySelectedDay = m_todayDay;

    m_dailyScrollIndex = 0;
    m_dailyFocus = 0;
    m_monthlyFocus = 0;
    m_selectedTitleIdx = 0;
    m_titleScrollTop = 0;

    if (initialTitleId != 0 && m_manager) {
        const auto& rankings = m_manager->allTimeRankings();
        for (size_t i = 0; i < rankings.size(); ++i) {
            if (rankings[i].titleId == initialTitleId) {
                m_selectedTitleIdx = static_cast<int>(i);
                m_titleScrollTop = std::max(0, m_selectedTitleIdx - 2);
                m_tabIndex = 2; // Jump to Software Library
                break;
            }
        }
    }

    m_focusArea = FocusArea::Content;
    rebuildTabBar();
    show();
}

void ActivityLogScreen::buildTabs() {
    auto& i18n = nxui::I18n::instance();
    m_tabs.clear();
    m_tabs.push_back({i18n.tr("activity_log.tab_daily", "Daily Log"), {}});
    m_tabs.push_back({i18n.tr("activity_log.tab_monthly", "Monthly Log"), {}});
    m_tabs.push_back({i18n.tr("activity_log.tab_titles", "Software Library"), {}});
}

void ActivityLogScreen::cycleTab(int delta) {
    if (m_tabs.empty()) return;
    int nextTab = m_tabIndex + delta;
    if (nextTab < 0) nextTab = static_cast<int>(m_tabs.size()) - 1;
    else if (nextTab >= static_cast<int>(m_tabs.size())) nextTab = 0;
    if (nextTab != m_tabIndex) {
        m_tabIndex = nextTab;
        if (m_tabChangeSfxCb) m_tabChangeSfxCb();
        m_focusArea = FocusArea::Content;
    }
}

void ActivityLogScreen::cycleDay(int delta) {
    m_curDay += delta;
    if (delta > 0) {
        int maxDays = switchu::activity::ActivityLogManager::daysInMonth(m_curYear, m_curMonth);
        if (m_curDay > maxDays) {
            m_curDay = 1;
            m_curMonth++;
            if (m_curMonth > 12) {
                m_curMonth = 1;
                m_curYear++;
            }
        }
    } else {
        if (m_curDay < 1) {
            m_curMonth--;
            if (m_curMonth < 1) {
                m_curMonth = 12;
                m_curYear--;
            }
            m_curDay = switchu::activity::ActivityLogManager::daysInMonth(m_curYear, m_curMonth);
        }
    }
    m_dailyScrollIndex = 0;
    if (m_dateChangeSfxCb) m_dateChangeSfxCb(delta > 0);
}

void ActivityLogScreen::cycleMonth(int delta) {
    m_monthMonth += delta;
    if (delta > 0) {
        if (m_monthMonth > 12) {
            m_monthMonth = 1;
            m_monthYear++;
        }
    } else {
        if (m_monthMonth < 1) {
            m_monthMonth = 12;
            m_monthYear--;
        }
    }
    int maxDays = switchu::activity::ActivityLogManager::daysInMonth(m_monthYear, m_monthMonth);
    if (m_monthlySelectedDay > maxDays)
        m_monthlySelectedDay = maxDays;
    if (m_dateChangeSfxCb) m_dateChangeSfxCb(delta > 0);
}

void ActivityLogScreen::jumpToToday() {
    bool changed = (m_curYear != m_todayYear || m_curMonth != m_todayMonth || m_curDay != m_todayDay ||
                    m_monthYear != m_todayYear || m_monthMonth != m_todayMonth);
    m_curYear = m_todayYear;
    m_curMonth = m_todayMonth;
    m_curDay = m_todayDay;
    m_monthYear = m_todayYear;
    m_monthMonth = m_todayMonth;
    m_monthlySelectedDay = m_todayDay;
    m_dailyScrollIndex = 0;
    if (changed && m_dateChangeSfxCb) m_dateChangeSfxCb(true);
}

void ActivityLogScreen::updateCustomContent(float) {
    // Keep selection within bounds
    if (m_manager) {
        const auto& rankings = m_manager->allTimeRankings();
        if (rankings.empty()) {
            m_selectedTitleIdx = 0;
            m_titleScrollTop = 0;
        } else {
            m_selectedTitleIdx = std::clamp(m_selectedTitleIdx, 0, static_cast<int>(rankings.size()) - 1);
            if (m_selectedTitleIdx < m_titleScrollTop) {
                m_titleScrollTop = m_selectedTitleIdx;
            } else if (m_selectedTitleIdx >= m_titleScrollTop + 5) {
                m_titleScrollTop = m_selectedTitleIdx - 4;
            }
        }
    }
}

void ActivityLogScreen::drawCustomContent(nxui::Renderer& ren, const nxui::Rect&,
                                         const nxui::Rect& content, float opacity) {
    if (opacity <= 0.01f) return;

    if (m_tabIndex == 0) {
        drawDailyTab(ren, content, opacity);
    } else if (m_tabIndex == 1) {
        drawMonthlyTab(ren, content, opacity);
    } else {
        drawTitlesTab(ren, content, opacity);
    }
}

void ActivityLogScreen::drawDailyTab(nxui::Renderer& ren, const nxui::Rect& content, float opacity) {
    auto& i18n = nxui::I18n::instance();
    const nxui::Color textPri = m_theme ? m_theme->textPrimary.withAlpha(opacity) : nxui::Color(1, 1, 1, opacity);
    const nxui::Color textSec = m_theme ? m_theme->textSecondary.withAlpha(opacity * 0.85f) : nxui::Color(0.8f, 0.8f, 0.8f, opacity);

    // 1. Calendar Navigation Header Bar
    nxui::Rect headerRect{content.x + 12.f, content.y + 10.f, content.width - 24.f, kHeaderH};
    ren.drawRoundedRect(headerRect, kCardBg.withAlpha(0.12f * opacity), 14.f);
    ren.drawRoundedRectOutline(headerRect, kColorCyan.withAlpha(0.25f * opacity), 14.f, 1.2f);

    // Left Arrow Button (<)
    nxui::Rect prevBtnRect{headerRect.x + 8.f, headerRect.y + 6.f, 44.f, 34.f};
    bool prevHovered = (m_focusArea == FocusArea::Content && m_dailyFocus == 0);
    ren.drawRoundedRect(prevBtnRect, (prevHovered ? kCardBgHighlight : kCardBg).withAlpha(0.2f * opacity), 10.f);
    ren.drawText("<", {prevBtnRect.x + 16.f, prevBtnRect.y + 6.f}, m_font, kColorCyan.withAlpha(opacity), 0.85f);

    // Date String in Center
    std::string dateStr = formatDateHeading(m_curYear, m_curMonth, m_curDay);
    float dateW = m_font ? m_font->measure(dateStr).x * 0.88f : 200.f;
    float dateX = headerRect.x + (headerRect.width - dateW) * 0.5f;
    ren.drawText(dateStr, {dateX, headerRect.y + 10.f}, m_font, textPri, 0.88f);

    // Right Arrow Button (>)
    nxui::Rect nextBtnRect{headerRect.right() - 150.f, headerRect.y + 6.f, 44.f, 34.f};
    ren.drawRoundedRect(nextBtnRect, (prevHovered ? kCardBgHighlight : kCardBg).withAlpha(0.2f * opacity), 10.f);
    ren.drawText(">", {nextBtnRect.x + 16.f, nextBtnRect.y + 6.f}, m_font, kColorCyan.withAlpha(opacity), 0.85f);

    // Today Badge / Button
    nxui::Rect todayBtnRect{headerRect.right() - 96.f, headerRect.y + 6.f, 88.f, 34.f};
    bool isToday = (m_curYear == m_todayYear && m_curMonth == m_todayMonth && m_curDay == m_todayDay);
    ren.drawRoundedRect(todayBtnRect,
                        isToday ? kColorGold.withAlpha(0.22f * opacity) : kCardBg.withAlpha(0.18f * opacity),
                        10.f);
    ren.drawRoundedRectOutline(todayBtnRect,
                               isToday ? kColorGold.withAlpha(0.7f * opacity) : kColorCyan.withAlpha(0.4f * opacity),
                               10.f, 1.0f);
    std::string todayLabel = isToday ? i18n.tr("activity_log.today", "Today") : "TODAY (Y)";
    ren.drawText(todayLabel, {todayBtnRect.x + 8.f, todayBtnRect.y + 8.f}, m_smallFont,
                 isToday ? kColorGold.withAlpha(opacity) : textSec, 0.65f);

    // 2. Day Summary Card
    nxui::Rect summaryRect{content.x + 12.f, headerRect.bottom() + 10.f, content.width - 24.f, kSummaryH};
    ren.drawRoundedRect(summaryRect, kCardBg.withAlpha(0.14f * opacity), 12.f);

    switchu::activity::DailyLogDay dayData = m_manager ? m_manager->queryDay(m_curYear, m_curMonth, m_curDay)
                                                       : switchu::activity::DailyLogDay{};

    // Total Playtime Pill
    std::string totalTimeStr = i18n.tr("activity_log.total_playtime", "Total Play Time") + ": " + formatPlaytime(dayData.totalPlaytimeSeconds);
    ren.drawText(totalTimeStr, {summaryRect.x + 20.f, summaryRect.y + 14.f}, m_font, kColorCyan.withAlpha(opacity), 0.80f);

    // Titles Played Pill
    std::string titlesCountStr = i18n.tr("activity_log.titles_played", "Titles Played") + ": " + std::to_string(dayData.titles.size());
    ren.drawText(titlesCountStr, {summaryRect.right() - 220.f, summaryRect.y + 14.f}, m_font, textSec, 0.80f);

    // 3. Bar Chart Area
    nxui::Rect listArea{content.x + 12.f, summaryRect.bottom() + 10.f, content.width - 24.f, content.bottom() - summaryRect.bottom() - 18.f};

    if (dayData.titles.empty()) {
        // Empty State Card
        nxui::Rect emptyCard{listArea.x + 40.f, listArea.y + 40.f, listArea.width - 80.f, 180.f};
        ren.drawRoundedRect(emptyCard, kCardBg.withAlpha(0.12f * opacity), 18.f);
        ren.drawRoundedRectOutline(emptyCard, nxui::Color(1, 1, 1, 0.15f * opacity), 18.f, 1.0f);

        std::string emptyMsg = i18n.tr("activity_log.empty_day", "No play records for this day.");
        std::string emptySub = i18n.tr("activity_log.empty_day_sub", "Play a game to see its daily record appear here!");
        std::string emptyNav = "Use [ZL]/[ZR] to change day, [Y] to jump to Today";

        ren.drawText(emptyMsg, {emptyCard.x + 30.f, emptyCard.y + 40.f}, m_font, textPri, 0.95f);
        ren.drawText(emptySub, {emptyCard.x + 30.f, emptyCard.y + 85.f}, m_smallFont, textSec, 0.75f);
        ren.drawText(emptyNav, {emptyCard.x + 30.f, emptyCard.y + 130.f}, m_smallFont, kColorCyan.withAlpha(opacity * 0.8f), 0.65f);
        return;
    }

    // Determine max playtime among titles today for proportional bars
    std::uint64_t maxPlaytime = 1;
    for (const auto& t : dayData.titles) {
        if (t.playtimeSeconds > maxPlaytime)
            maxPlaytime = t.playtimeSeconds;
    }

    ren.pushClipRect(listArea);

    int visibleCount = std::min<int>(5, static_cast<int>(dayData.titles.size()) - m_dailyScrollIndex);
    float curY = listArea.y;

    for (int i = 0; i < visibleCount; ++i) {
        int idx = m_dailyScrollIndex + i;
        const auto& entry = dayData.titles[idx];
        nxui::Rect rowRect{listArea.x, curY, listArea.width, kDailyItemH};

        // Glass background card
        ren.drawRoundedRect(rowRect, kCardBg.withAlpha(0.16f * opacity), 12.f);

        // Color badge for rank
        nxui::Color titleColor = getTitleColor(idx);
        nxui::Rect badgeRect{rowRect.x + 12.f, rowRect.y + 12.f, 38.f, 38.f};
        ren.drawRoundedRect(badgeRect, titleColor.withAlpha(0.85f * opacity), 10.f);

        std::string rankStr = "#" + std::to_string(idx + 1);
        ren.drawText(rankStr, {badgeRect.x + 6.f, badgeRect.y + 10.f}, m_smallFont, nxui::Color(1, 1, 1, opacity), 0.65f);

        // Title Name
        float textW = rowRect.width - 240.f;
        std::string displayTitle = ellipsizeText(m_font, entry.titleName.empty() ? ("Title " + std::to_string(entry.titleId)) : entry.titleName, textW, 0.80f);
        ren.drawText(displayTitle, {rowRect.x + 58.f, rowRect.y + 10.f}, m_font, textPri, 0.80f);

        // Playtime and launches text
        std::string timeStr = formatPlaytime(entry.playtimeSeconds);
        if (entry.launches > 1) {
            timeStr += " (" + std::to_string(entry.launches) + "x)";
        }
        float timeW = m_font ? m_font->measure(timeStr).x * 0.75f : 100.f;
        ren.drawText(timeStr, {rowRect.right() - timeW - 16.f, rowRect.y + 12.f}, m_font, titleColor.withAlpha(opacity), 0.75f);

        // Bar Chart Track & Fill
        float barX = rowRect.x + 58.f;
        float barY = rowRect.y + 40.f;
        float barMaxW = rowRect.width - 76.f;
        float barH = 10.f;
        nxui::Rect trackRect{barX, barY, barMaxW, barH};
        ren.drawRoundedRect(trackRect, kBarTrack, 5.f);

        float fillRatio = std::clamp(static_cast<float>(entry.playtimeSeconds) / static_cast<float>(maxPlaytime), 0.04f, 1.0f);
        nxui::Rect fillRect{barX, barY, barMaxW * fillRatio, barH};
        ren.drawRoundedRect(fillRect, titleColor.withAlpha(0.95f * opacity), 5.f);

        curY += kDailyItemH + kItemGap;
    }

    ren.popClipRect();
}

void ActivityLogScreen::drawMonthlyTab(nxui::Renderer& ren, const nxui::Rect& content, float opacity) {
    auto& i18n = nxui::I18n::instance();
    const nxui::Color textPri = m_theme ? m_theme->textPrimary.withAlpha(opacity) : nxui::Color(1, 1, 1, opacity);
    const nxui::Color textSec = m_theme ? m_theme->textSecondary.withAlpha(opacity * 0.85f) : nxui::Color(0.8f, 0.8f, 0.8f, opacity);

    // 1. Month Navigation Header
    nxui::Rect headerRect{content.x + 12.f, content.y + 10.f, content.width - 24.f, kHeaderH};
    ren.drawRoundedRect(headerRect, kCardBg.withAlpha(0.12f * opacity), 14.f);
    ren.drawRoundedRectOutline(headerRect, kColorCyan.withAlpha(0.25f * opacity), 14.f, 1.2f);

    // Left Arrow (<)
    nxui::Rect prevBtnRect{headerRect.x + 8.f, headerRect.y + 6.f, 44.f, 34.f};
    ren.drawRoundedRect(prevBtnRect, kCardBg.withAlpha(0.2f * opacity), 10.f);
    ren.drawText("<", {prevBtnRect.x + 16.f, prevBtnRect.y + 6.f}, m_font, kColorCyan.withAlpha(opacity), 0.85f);

    // Month Heading
    std::string monthStr = formatMonthHeading(m_monthYear, m_monthMonth);
    float monthW = m_font ? m_font->measure(monthStr).x * 0.88f : 180.f;
    float monthX = headerRect.x + (headerRect.width - monthW) * 0.5f;
    ren.drawText(monthStr, {monthX, headerRect.y + 10.f}, m_font, textPri, 0.88f);

    // Right Arrow (>)
    nxui::Rect nextBtnRect{headerRect.right() - 52.f, headerRect.y + 6.f, 44.f, 34.f};
    ren.drawRoundedRect(nextBtnRect, kCardBg.withAlpha(0.2f * opacity), 10.f);
    ren.drawText(">", {nextBtnRect.x + 16.f, nextBtnRect.y + 6.f}, m_font, kColorCyan.withAlpha(opacity), 0.85f);

    // 2. Month Summary Card
    nxui::Rect summaryRect{content.x + 12.f, headerRect.bottom() + 10.f, content.width - 24.f, kSummaryH};
    ren.drawRoundedRect(summaryRect, kCardBg.withAlpha(0.14f * opacity), 12.f);

    switchu::activity::MonthlyLogSummary monthData = m_manager ? m_manager->queryMonth(m_monthYear, m_monthMonth)
                                                               : switchu::activity::MonthlyLogSummary{};

    std::string monthTotalStr = i18n.tr("activity_log.total_playtime", "Total Play Time") + ": " + formatPlaytime(monthData.totalPlaytimeSeconds);
    ren.drawText(monthTotalStr, {summaryRect.x + 20.f, summaryRect.y + 14.f}, m_font, kColorCyan.withAlpha(opacity), 0.80f);

    std::string activeDaysStr = i18n.tr("activity_log.active_days", "Active Days") + ": " + std::to_string(monthData.activeDaysCount);
    ren.drawText(activeDaysStr, {summaryRect.right() - 200.f, summaryRect.y + 14.f}, m_font, textSec, 0.80f);

    // 3. Daily Play Time Distribution Graph (Wii U Calendar style)
    nxui::Rect graphCard{content.x + 12.f, summaryRect.bottom() + 10.f, content.width - 24.f, 175.f};
    ren.drawRoundedRect(graphCard, kCardBg.withAlpha(0.16f * opacity), 14.f);

    std::string distTitle = i18n.tr("activity_log.daily_distribution", "Daily Play Time Distribution");
    ren.drawText(distTitle, {graphCard.x + 16.f, graphCard.y + 12.f}, m_smallFont, textSec, 0.70f);

    int daysInM = switchu::activity::ActivityLogManager::daysInMonth(m_monthYear, m_monthMonth);
    std::uint64_t maxDayPlaytime = 1;
    for (int d = 0; d < daysInM; ++d) {
        if (monthData.dailyPlaytimeSeconds[d] > maxDayPlaytime)
            maxDayPlaytime = monthData.dailyPlaytimeSeconds[d];
    }

    float barAreaX = graphCard.x + 18.f;
    float barAreaW = graphCard.width - 36.f;
    float slotW = barAreaW / static_cast<float>(daysInM);
    float barW = std::max(4.f, slotW - 3.f);
    float baseY = graphCard.y + 145.f;
    float maxBarH = 95.f;

    // Baseline axis
    ren.drawRect({barAreaX, baseY + 1.f, barAreaW, 1.f}, nxui::Color(1, 1, 1, 0.2f * opacity));

    for (int d = 1; d <= daysInM; ++d) {
        float cx = barAreaX + (d - 1) * slotW + (slotW - barW) * 0.5f;
        std::uint64_t sec = monthData.dailyPlaytimeSeconds[d - 1];

        bool isTodayDay = (m_monthYear == m_todayYear && m_monthMonth == m_todayMonth && d == m_todayDay);

        if (sec > 0) {
            float barH = std::clamp((static_cast<float>(sec) / static_cast<float>(maxDayPlaytime)) * maxBarH, 6.f, maxBarH);
            nxui::Rect dayBarRect{cx, baseY - barH, barW, barH};
            ren.drawRoundedRect(dayBarRect, isTodayDay ? kColorGold.withAlpha(0.95f * opacity) : kColorCyan.withAlpha(0.85f * opacity), 3.f);
        }

        // Today indicator dot
        if (isTodayDay) {
            ren.drawCircle({cx + barW * 0.5f, baseY + 8.f}, 2.5f, kColorGold.withAlpha(opacity));
        }

        // Axis day labels (every 5 days and day 1)
        if (d == 1 || d == 5 || d == 10 || d == 15 || d == 20 || d == 25 || d == daysInM) {
            std::string dLabel = std::to_string(d);
            ren.drawText(dLabel, {cx - 2.f, baseY + 14.f}, m_smallFont, textSec, 0.50f);
        }
    }

    // 4. Top Software Played this Month
    nxui::Rect topTitlesArea{content.x + 12.f, graphCard.bottom() + 10.f, content.width - 24.f, content.bottom() - graphCard.bottom() - 14.f};
    ren.pushClipRect(topTitlesArea);

    if (monthData.titles.empty()) {
        std::string noData = i18n.tr("activity_log.empty_month", "No play records for this month.");
        ren.drawText(noData, {topTitlesArea.x + 20.f, topTitlesArea.y + 20.f}, m_smallFont, textSec, 0.75f);
    } else {
        int count = std::min<int>(3, static_cast<int>(monthData.titles.size()));
        float ty = topTitlesArea.y;
        for (int i = 0; i < count; ++i) {
            const auto& t = monthData.titles[i];
            nxui::Rect rowRect{topTitlesArea.x, ty, topTitlesArea.width, 42.f};
            ren.drawRoundedRect(rowRect, kCardBg.withAlpha(0.12f * opacity), 10.f);

            nxui::Color color = getTitleColor(i);
            std::string rank = "#" + std::to_string(i + 1);
            ren.drawText(rank, {rowRect.x + 12.f, rowRect.y + 10.f}, m_smallFont, color.withAlpha(opacity), 0.70f);

            std::string name = ellipsizeText(m_font, t.titleName.empty() ? ("Title " + std::to_string(t.titleId)) : t.titleName, rowRect.width - 220.f, 0.75f);
            ren.drawText(name, {rowRect.x + 46.f, rowRect.y + 10.f}, m_font, textPri, 0.75f);

            std::string pt = formatPlaytime(t.playtimeSeconds);
            float ptW = m_font ? m_font->measure(pt).x * 0.72f : 80.f;
            ren.drawText(pt, {rowRect.right() - ptW - 14.f, rowRect.y + 10.f}, m_font, color.withAlpha(opacity), 0.72f);

            ty += 48.f;
        }
    }

    ren.popClipRect();
}

void ActivityLogScreen::drawTitlesTab(nxui::Renderer& ren, const nxui::Rect& content, float opacity) {
    auto& i18n = nxui::I18n::instance();
    const nxui::Color textPri = m_theme ? m_theme->textPrimary.withAlpha(opacity) : nxui::Color(1, 1, 1, opacity);
    const nxui::Color textSec = m_theme ? m_theme->textSecondary.withAlpha(opacity * 0.85f) : nxui::Color(0.8f, 0.8f, 0.8f, opacity);

    // Header Bar
    nxui::Rect headerRect{content.x + 12.f, content.y + 10.f, content.width - 24.f, 38.f};
    std::string libTitle = i18n.tr("activity_log.tab_titles", "Software Library") + " - " + i18n.tr("activity_log.all_time", "All-Time Rankings");
    ren.drawText(libTitle, {headerRect.x + 8.f, headerRect.y + 8.f}, m_font, kColorCyan.withAlpha(opacity), 0.85f);

    const auto& rankings = m_manager ? m_manager->allTimeRankings() : std::vector<switchu::activity::TitlePlayStats>{};

    if (rankings.empty()) {
        nxui::Rect emptyCard{content.x + 40.f, content.y + 80.f, content.width - 80.f, 150.f};
        ren.drawRoundedRect(emptyCard, kCardBg.withAlpha(0.12f * opacity), 18.f);
        std::string emptyMsg = i18n.tr("activity_log.empty_titles", "No software play history recorded yet.");
        ren.drawText(emptyMsg, {emptyCard.x + 30.f, emptyCard.y + 55.f}, m_font, textPri, 0.90f);
        return;
    }

    nxui::Rect listArea{content.x + 12.f, headerRect.bottom() + 8.f, content.width - 24.f, content.bottom() - headerRect.bottom() - 14.f};
    ren.pushClipRect(listArea);

    int totalItems = static_cast<int>(rankings.size());
    int visibleItems = std::min<int>(5, totalItems - m_titleScrollTop);
    float curY = listArea.y;

    std::uint64_t maxTotalPlaytime = rankings.front().totalPlaytimeSeconds > 0 ? rankings.front().totalPlaytimeSeconds : 1;

    for (int i = 0; i < visibleItems; ++i) {
        int idx = m_titleScrollTop + i;
        const auto& stats = rankings[idx];
        bool isFocused = (m_focusArea == FocusArea::Content && idx == m_selectedTitleIdx);

        nxui::Rect cardRect{listArea.x, curY, listArea.width, kTitleItemH};

        // Background card
        ren.drawRoundedRect(cardRect, (isFocused ? kCardBgHighlight : kCardBg).withAlpha(0.18f * opacity), 12.f);

        if (isFocused) {
            ren.drawRoundedRectOutline(cardRect, kColorCyan.withAlpha(0.75f * opacity), 12.f, 2.0f);
        }

        // Rank Medallion (1st: Gold, 2nd: Silver, 3rd: Bronze, 4+: Neutral)
        nxui::Color medalColor = (idx == 0) ? kColorGold : ((idx == 1) ? kColorSilver : ((idx == 2) ? kColorBronze : nxui::Color(0.5f, 0.6f, 0.7f, 1.0f)));
        nxui::Rect medalRect{cardRect.x + 12.f, cardRect.y + 14.f, 40.f, 40.f};
        ren.drawRoundedRect(medalRect, medalColor.withAlpha(0.25f * opacity), 10.f);
        ren.drawRoundedRectOutline(medalRect, medalColor.withAlpha(0.85f * opacity), 10.f, 1.5f);

        std::string rankStr = "#" + std::to_string(idx + 1);
        float rw = m_smallFont ? m_smallFont->measure(rankStr).x * 0.70f : 20.f;
        ren.drawText(rankStr, {medalRect.x + (medalRect.width - rw) * 0.5f, medalRect.y + 11.f}, m_smallFont, medalColor.withAlpha(opacity), 0.70f);

        // Title Name
        float nameMaxW = cardRect.width - 250.f;
        std::string nameStr = ellipsizeText(m_font, stats.titleName.empty() ? ("Title " + std::to_string(stats.titleId)) : stats.titleName, nameMaxW, 0.82f);
        ren.drawText(nameStr, {cardRect.x + 62.f, cardRect.y + 10.f}, m_font, textPri, 0.82f);

        // Statistics row below title name: Times Played, Average Session, First Played, Last Played
        std::string statRow = i18n.tr("activity_log.times_played", "Times Played") + ": " + std::to_string(stats.totalLaunches) +
                              "  |  " + i18n.tr("activity_log.avg_playtime", "Avg Session") + ": " + formatAverageSession(stats.averageSessionSeconds);
        if (stats.lastPlayedTimestamp > 0) {
            statRow += "  |  " + i18n.tr("activity_log.last_played", "Last") + ": " + formatDateNumeric(stats.lastPlayedTimestamp);
        }
        ren.drawText(statRow, {cardRect.x + 62.f, cardRect.y + 36.f}, m_smallFont, textSec, 0.58f);

        // Total Playtime text on right
        std::string totalStr = formatPlaytime(stats.totalPlaytimeSeconds);
        float tw = m_font ? m_font->measure(totalStr).x * 0.82f : 80.f;
        ren.drawText(totalStr, {cardRect.right() - tw - 16.f, cardRect.y + 12.f}, m_font, medalColor.withAlpha(opacity), 0.82f);

        // Proportional progress bar
        float pbX = cardRect.x + 62.f;
        float pbY = cardRect.y + 54.f;
        float pbW = cardRect.width - 80.f;
        ren.drawRoundedRect({pbX, pbY, pbW, 6.f}, kBarTrack, 3.f);

        float fillW = std::clamp((static_cast<float>(stats.totalPlaytimeSeconds) / static_cast<float>(maxTotalPlaytime)) * pbW, 6.f, pbW);
        ren.drawRoundedRect({pbX, pbY, fillW, 6.f}, medalColor.withAlpha(0.9f * opacity), 3.f);

        curY += kTitleItemH + kItemGap;
    }

    ren.popClipRect();
}

bool ActivityLogScreen::handleCustomPressA() {
    if (m_focusArea == FocusArea::Tabs) {
        m_focusArea = FocusArea::Content;
        if (m_activateSfxCb) m_activateSfxCb();
        return true;
    }

    if (m_tabIndex == 2 && m_manager) {
        const auto& rankings = m_manager->allTimeRankings();
        if (m_selectedTitleIdx >= 0 && m_selectedTitleIdx < static_cast<int>(rankings.size())) {
            if (m_titleSelectedCb) {
                if (m_activateSfxCb) m_activateSfxCb();
                m_titleSelectedCb(rankings[m_selectedTitleIdx].titleId);
                return true;
            }
        }
    }
    return false;
}

bool ActivityLogScreen::handleCustomPressB() {
    if (m_focusArea == FocusArea::Content) {
        m_focusArea = FocusArea::Tabs;
        if (m_navSfxCb) m_navSfxCb();
        return true;
    }
    hide();
    if (m_closeCb) m_closeCb();
    return true;
}

bool ActivityLogScreen::handleCustomPressX() {
    jumpToToday();
    return true;
}

bool ActivityLogScreen::handleCustomNavUp() {
    if (m_tabIndex == 0) {
        if (m_dailyScrollIndex > 0) {
            m_dailyScrollIndex--;
            if (m_navSfxCb) m_navSfxCb();
            return true;
        }
    } else if (m_tabIndex == 2 && m_manager) {
        if (m_selectedTitleIdx > 0) {
            m_selectedTitleIdx--;
            if (m_navSfxCb) m_navSfxCb();
            return true;
        }
    }
    return false;
}

bool ActivityLogScreen::handleCustomNavDown() {
    if (m_tabIndex == 0 && m_manager) {
        auto day = m_manager->queryDay(m_curYear, m_curMonth, m_curDay);
        if (m_dailyScrollIndex + 5 < static_cast<int>(day.titles.size())) {
            m_dailyScrollIndex++;
            if (m_navSfxCb) m_navSfxCb();
            return true;
        }
    } else if (m_tabIndex == 2 && m_manager) {
        const auto& rankings = m_manager->allTimeRankings();
        if (m_selectedTitleIdx + 1 < static_cast<int>(rankings.size())) {
            m_selectedTitleIdx++;
            if (m_navSfxCb) m_navSfxCb();
            return true;
        }
    }
    return false;
}

bool ActivityLogScreen::handleCustomNavLeft() {
    if (m_focusArea == FocusArea::Tabs)
        return false;

    if (m_tabIndex == 0) {
        cycleDay(-1);
        return true;
    } else if (m_tabIndex == 1) {
        cycleMonth(-1);
        return true;
    } else {
        // Move focus back to tab rail
        m_focusArea = FocusArea::Tabs;
        if (m_navSfxCb) m_navSfxCb();
        return true;
    }
}

bool ActivityLogScreen::handleCustomNavRight() {
    if (m_tabIndex == 0) {
        cycleDay(1);
        return true;
    } else if (m_tabIndex == 1) {
        cycleMonth(1);
        return true;
    }
    return false;
}

bool ActivityLogScreen::handleCustomTouch(nxui::Input& input, const nxui::Rect&,
                                         const nxui::Rect&, const nxui::Rect& content) {
    if (input.touchDown()) {
        float tx = input.touchX();
        float ty = input.touchY();

        nxui::Rect headerRect{content.x + 12.f, content.y + 10.f, content.width - 24.f, kHeaderH};
        nxui::Rect prevBtnRect{headerRect.x + 8.f, headerRect.y + 6.f, 44.f, 34.f};
        nxui::Rect nextBtnRect{headerRect.right() - (m_tabIndex == 0 ? 150.f : 52.f), headerRect.y + 6.f, 44.f, 34.f};
        nxui::Rect todayBtnRect{headerRect.right() - 96.f, headerRect.y + 6.f, 88.f, 34.f};

        if (prevBtnRect.contains(tx, ty)) {
            m_touchDownTarget = 0;
            return true;
        } else if (nextBtnRect.contains(tx, ty)) {
            m_touchDownTarget = 1;
            return true;
        } else if (m_tabIndex == 0 && todayBtnRect.contains(tx, ty)) {
            m_touchDownTarget = 2;
            return true;
        }
        return content.contains(tx, ty);
    }

    if (input.touchUp()) {
        int target = m_touchDownTarget;
        m_touchDownTarget = -1;

        if (target == 0) {
            if (m_tabIndex == 0) cycleDay(-1);
            else if (m_tabIndex == 1) cycleMonth(-1);
            return true;
        } else if (target == 1) {
            if (m_tabIndex == 0) cycleDay(1);
            else if (m_tabIndex == 1) cycleMonth(1);
            return true;
        } else if (target == 2) {
            jumpToToday();
            return true;
        }
    }

    return false;
}

std::string ActivityLogScreen::currentAccessibilitySummary() const {
    auto& i18n = nxui::I18n::instance();
    if (m_tabIndex == 0) {
        std::string res = i18n.tr("activity_log.tab_daily", "Daily Log") + ", " + formatDateHeading(m_curYear, m_curMonth, m_curDay);
        if (m_manager) {
            auto d = m_manager->queryDay(m_curYear, m_curMonth, m_curDay);
            res += ". " + i18n.tr("activity_log.total_playtime", "Total Play Time") + ": " + formatPlaytime(d.totalPlaytimeSeconds);
        }
        return res;
    } else if (m_tabIndex == 1) {
        std::string res = i18n.tr("activity_log.tab_monthly", "Monthly Log") + ", " + formatMonthHeading(m_monthYear, m_monthMonth);
        if (m_manager) {
            auto m = m_manager->queryMonth(m_monthYear, m_monthMonth);
            res += ". " + i18n.tr("activity_log.total_playtime", "Total Play Time") + ": " + formatPlaytime(m.totalPlaytimeSeconds);
        }
        return res;
    } else {
        std::string res = i18n.tr("activity_log.tab_titles", "Software Library");
        if (m_manager) {
            const auto& r = m_manager->allTimeRankings();
            if (m_selectedTitleIdx >= 0 && m_selectedTitleIdx < static_cast<int>(r.size())) {
                const auto& s = r[m_selectedTitleIdx];
                res += ". #" + std::to_string(m_selectedTitleIdx + 1) + " " + s.titleName + ", " + formatPlaytime(s.totalPlaytimeSeconds);
            }
        }
        return res;
    }
}

std::string ActivityLogScreen::formatPlaytime(std::uint64_t seconds) const {
    if (seconds == 0) return "0 min";
    std::uint64_t minutes = seconds / 60;
    if (minutes < 60) return std::to_string(minutes) + " min";
    std::uint64_t hours = minutes / 60;
    std::uint64_t remM = minutes % 60;
    if (remM == 0) return std::to_string(hours) + " h";
    return std::to_string(hours) + " h " + std::to_string(remM) + " min";
}

std::string ActivityLogScreen::formatAverageSession(std::uint64_t seconds) const {
    if (seconds == 0) return "0 min";
    std::uint64_t minutes = seconds / 60;
    if (minutes < 60) return std::to_string(minutes) + " min";
    return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
}

std::string ActivityLogScreen::formatDateHeading(int y, int m, int d) const {
    int dow = dayOfWeek(y, m, d);
    const char* dayName = (dow >= 0 && dow < 7) ? kDayNamesEn[dow] : "";
    const char* monthName = (m >= 1 && m <= 12) ? kMonthNamesEn[m - 1] : "";
    std::ostringstream ss;
    ss << dayName << ", " << monthName << " " << d << ", " << y;
    return ss.str();
}

std::string ActivityLogScreen::formatMonthHeading(int y, int m) const {
    const char* monthName = (m >= 1 && m <= 12) ? kMonthNamesEn[m - 1] : "";
    std::ostringstream ss;
    ss << monthName << " " << y;
    return ss.str();
}

std::string ActivityLogScreen::formatDateNumeric(std::uint64_t posixSeconds) const {
    if (posixSeconds == 0) return "—";
    std::time_t raw = static_cast<std::time_t>(posixSeconds);
    std::tm t{};
#if defined(_WIN32) && !defined(__SWITCH__)
    localtime_s(&t, &raw);
#else
    localtime_r(&raw, &t);
#endif
    std::ostringstream ss;
    ss << (t.tm_year + 1900) << "-"
       << std::setw(2) << std::setfill('0') << (t.tm_mon + 1) << "-"
       << std::setw(2) << std::setfill('0') << t.tm_mday;
    return ss.str();
}

nxui::Color ActivityLogScreen::getTitleColor(size_t index) const {
    constexpr size_t numColors = sizeof(kTitleBarColors) / sizeof(kTitleBarColors[0]);
    return kTitleBarColors[index % numColors];
}
