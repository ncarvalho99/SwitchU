#include "GameCheatsScreen.hpp"

#include <fmt/format.h>
#include <nxui/core/I18n.hpp>
#include <nxui/core/Renderer.hpp>

#include <algorithm>

GameCheatsScreen::GameCheatsScreen()
    : TabbedOverlayScreen(ScreenMode::ThemeShop) {
}

void GameCheatsScreen::openForGame(std::uint64_t titleId, std::string title) {
    m_titleId = titleId;
    m_title = std::move(title);
    m_currentBuild = 0;
    m_selected = 0;
    reload();
    show();
    m_focusArea = FocusArea::Content;
    addAction(static_cast<uint64_t>(nxui::Button::L),  [this]() { switchBuild(-1); });
    addAction(static_cast<uint64_t>(nxui::Button::R),  [this]() { switchBuild(1); });
    addAction(static_cast<uint64_t>(nxui::Button::ZL), [this]() { switchBuild(-1); });
    addAction(static_cast<uint64_t>(nxui::Button::ZR), [this]() { switchBuild(1); });
}

void GameCheatsScreen::buildTabs() {
    m_tabs.clear();
    m_tabs.push_back({"", {}});
}

void GameCheatsScreen::reload() {
    m_builds = cheats::GameCheatManager::load(m_titleId);
    if (m_currentBuild < 0 || m_currentBuild >= static_cast<int>(m_builds.size())) {
        m_currentBuild = 0;
    }
    const int count = currentCheatsCount();
    m_selected = std::clamp(m_selected, 0, std::max(0, count - 1));
}

const cheats::BuildCheats* GameCheatsScreen::currentBuildCheats() const {
    if (m_builds.empty() || m_currentBuild < 0 || m_currentBuild >= static_cast<int>(m_builds.size())) {
        return nullptr;
    }
    return &m_builds[static_cast<size_t>(m_currentBuild)];
}

cheats::BuildCheats* GameCheatsScreen::currentBuildCheats() {
    if (m_builds.empty() || m_currentBuild < 0 || m_currentBuild >= static_cast<int>(m_builds.size())) {
        return nullptr;
    }
    return &m_builds[static_cast<size_t>(m_currentBuild)];
}

int GameCheatsScreen::currentCheatsCount() const {
    const auto* build = currentBuildCheats();
    return build ? static_cast<int>(build->cheats.size()) : 0;
}

void GameCheatsScreen::switchBuild(int delta) {
    if (m_builds.size() <= 1) return;
    const int count = static_cast<int>(m_builds.size());
    m_currentBuild = (m_currentBuild + delta + count) % count;
    m_selected = 0;
    if (m_navSfxCb) m_navSfxCb();
}

void GameCheatsScreen::toggleCurrent() {
    auto* build = currentBuildCheats();
    if (!build || build->cheats.empty()) return;
    if (m_selected < 0 || m_selected >= static_cast<int>(build->cheats.size())) return;

    auto& cheat = build->cheats[static_cast<size_t>(m_selected)];
    cheat.enabled = !cheat.enabled;

    if (cheat.enabled) {
        if (m_activateSfxCb) m_activateSfxCb();
    } else {
        if (m_toggleOffSfxCb) m_toggleOffSfxCb();
        else if (m_activateSfxCb) m_activateSfxCb();
    }

    cheats::GameCheatManager::saveToggles(m_titleId, build->cheats);

    auto& i18n = nxui::I18n::instance();
    const std::string state = cheat.enabled
        ? i18n.tr("dialog.cheats_active", "Enabled")
        : i18n.tr("dialog.cheats_disabled", "Disabled");
    requestToast(cheat.name + ": " + state);
}

void GameCheatsScreen::toggleAll() {
    auto* build = currentBuildCheats();
    if (!build || build->cheats.empty()) return;

    bool anyEnabled = false;
    for (const auto& c : build->cheats) {
        if (!c.isMaster && c.enabled) {
            anyEnabled = true;
            break;
        }
    }

    const bool targetState = !anyEnabled;
    for (auto& c : build->cheats) {
        if (!c.isMaster) {
            c.enabled = targetState;
        }
    }

    if (targetState) {
        if (m_activateSfxCb) m_activateSfxCb();
    } else {
        if (m_toggleOffSfxCb) m_toggleOffSfxCb();
        else if (m_activateSfxCb) m_activateSfxCb();
    }

    cheats::GameCheatManager::saveToggles(m_titleId, build->cheats);

    auto& i18n = nxui::I18n::instance();
    requestToast(i18n.tr(targetState ? "dialog.cheats_enabled_all_toast" : "dialog.cheats_disabled_all_toast",
                          targetState ? "All cheats enabled." : "All cheats disabled."));
}

bool GameCheatsScreen::handleCustomPressA() {
    toggleCurrent();
    return true;
}

bool GameCheatsScreen::handleCustomPressB() {
    hide();
    return true;
}

bool GameCheatsScreen::handleCustomPressX() {
    toggleAll();
    return true;
}

bool GameCheatsScreen::handleCustomNavUp() {
    const int count = currentCheatsCount();
    if (count > 0) {
        m_selected = (m_selected - 1 + count) % count;
        if (m_navSfxCb) m_navSfxCb();
    }
    return true;
}

bool GameCheatsScreen::handleCustomNavDown() {
    const int count = currentCheatsCount();
    if (count > 0) {
        m_selected = (m_selected + 1) % count;
        if (m_navSfxCb) m_navSfxCb();
    }
    return true;
}

bool GameCheatsScreen::handleCustomNavLeft() {
    if (m_builds.size() > 1) {
        switchBuild(-1);
    }
    return true;
}

bool GameCheatsScreen::handleCustomNavRight() {
    if (m_builds.size() > 1) {
        switchBuild(1);
    }
    return true;
}

std::string GameCheatsScreen::currentAccessibilitySummary() const {
    auto& i18n = nxui::I18n::instance();
    if (m_builds.empty()) {
        return i18n.tr("dialog.cheats_empty", "No cheats found for this game.");
    }
    const auto* build = currentBuildCheats();
    if (!build || build->cheats.empty()) {
        return i18n.tr("dialog.cheats_empty", "No cheats found for this game.");
    }
    if (m_selected < 0 || m_selected >= static_cast<int>(build->cheats.size())) {
        return m_title;
    }
    const auto& cheat = build->cheats[static_cast<size_t>(m_selected)];
    const std::string state = cheat.enabled
        ? i18n.tr("dialog.cheats_active", "Enabled")
        : i18n.tr("dialog.cheats_disabled", "Disabled");
    return cheat.name + ". " + state + ". " + std::to_string(cheat.lines.size()) + " " +
           i18n.tr("dialog.cheats_opcodes", "lines");
}

void GameCheatsScreen::drawCustomContent(nxui::Renderer& ren, const nxui::Rect& panel,
                                        const nxui::Rect&, float opacity) {
    if (!m_theme || !m_font || !m_smallFont) return;
    auto& i18n = nxui::I18n::instance();
    const auto primary = m_theme->textPrimary.withAlpha(opacity);
    const auto secondary = m_theme->textSecondary.withAlpha(0.82f * opacity);
    const auto subtle = m_theme->textSecondary.withAlpha(0.58f * opacity);
    const nxui::Rect body = panel.shrunk(28.f);

    // Title
    ren.drawText(i18n.tr("dialog.cheats_title", "Cheats") + " — " + m_title,
                 {body.x + 12.f, body.y + 12.f}, m_font, primary, 1.02f);

    const auto* build = currentBuildCheats();
    if (build) {
        int activeCount = 0;
        for (const auto& c : build->cheats) {
            if (c.enabled) ++activeCount;
        }

        std::string buildInfo;
        if (m_builds.size() > 1) {
            buildInfo = fmt::format("{} {}/{} ({}): {}/{} {}",
                                    i18n.tr("dialog.cheats_build_id", "Build"),
                                    m_currentBuild + 1, m_builds.size(),
                                    build->buildId,
                                    activeCount, build->cheats.size(),
                                    i18n.tr("dialog.cheats_active_summary", "active"));
        } else {
            buildInfo = fmt::format("{}: {}  •  {}/{} {}",
                                    i18n.tr("dialog.cheats_build_id", "Build"),
                                    build->buildId,
                                    activeCount, build->cheats.size(),
                                    i18n.tr("dialog.cheats_active_summary", "active"));
        }
        ren.drawText(buildInfo, {body.right() - 440.f, body.y + 16.f}, m_smallFont, secondary, 0.68f);
    }

    // List Container
    const nxui::Rect list = {body.x + 10.f, body.y + 54.f, body.width - 20.f, body.height - 150.f};
    ren.drawRoundedRect(list, m_theme->panelBase.withAlpha(0.07f * opacity), 18.f);
    ren.drawRoundedRectOutline(list, m_theme->panelBorder.withAlpha(0.16f * opacity), 18.f, 1.f);

    if (!build || build->cheats.empty()) {
        ren.drawText(i18n.tr("dialog.cheats_empty", "No cheats found for this game."),
                     {list.x + 24.f, list.y + 36.f}, m_font, primary, 0.88f);
        ren.drawText(i18n.tr("dialog.cheats_empty_path", "Place Atmosphere cheat files in:"),
                     {list.x + 24.f, list.y + 74.f}, m_smallFont, secondary, 0.72f);
        const std::string expectedPath = fmt::format("sdmc:/atmosphere/contents/{:016X}/cheats/<buildId>.txt", m_titleId);
        ren.drawText(expectedPath, {list.x + 24.f, list.y + 102.f}, m_smallFont,
                     m_theme->cursorNormal.withAlpha(0.9f * opacity), 0.70f);
        return;
    }

    const auto& cheats = build->cheats;
    constexpr int visible = 5;
    const int start = std::clamp(m_selected - visible / 2, 0, std::max(0, static_cast<int>(cheats.size()) - visible));
    const int end = std::min(static_cast<int>(cheats.size()), start + visible);
    constexpr float rowH = 68.f;

    for (int i = start; i < end; ++i) {
        const auto& cheat = cheats[static_cast<size_t>(i)];
        const nxui::Rect row = {list.x + 14.f, list.y + 12.f + (i - start) * (rowH + 8.f),
                                list.width - 28.f, rowH};
        const bool selected = (i == m_selected);

        ren.drawRoundedRect(row, m_theme->panelBase.withAlpha((selected ? 0.20f : 0.055f) * opacity), 13.f);
        ren.drawRoundedRectOutline(row, (selected ? m_theme->cursorNormal : m_theme->panelBorder)
                                  .withAlpha((selected ? 0.85f : 0.14f) * opacity), 13.f,
                                  selected ? 2.f : 1.f);

        // Cheat Name
        ren.drawText(cheat.name, {row.x + 18.f, row.y + 12.f}, m_font, primary, 0.78f);

        // Subtitle / tags
        if (cheat.isMaster) {
            ren.drawText(i18n.tr("dialog.cheats_master", "Master Code"),
                         {row.x + 18.f, row.y + 40.f}, m_smallFont,
                         nxui::Color(0.95f, 0.65f, 0.15f, opacity), 0.64f);
        } else {
            const std::string lineCountStr = fmt::format("{} {}", cheat.lines.size(),
                                                        i18n.tr("dialog.cheats_opcodes", "lines"));
            ren.drawText(lineCountStr, {row.x + 18.f, row.y + 40.f}, m_smallFont, subtle, 0.62f);
        }

        // Toggle Switch Widget
        constexpr float toggleW = 58.f;
        constexpr float toggleH = 28.f;
        constexpr float knobSize = 22.f;
        const nxui::Rect toggle = {row.right() - toggleW - 18.f,
                                   row.y + (row.height - toggleH) * 0.5f,
                                   toggleW, toggleH};
        const nxui::Color offColor(0.42f, 0.44f, 0.48f, 1.f);
        const nxui::Color onColor(0.12f, 0.72f, 0.42f, 1.f);

        ren.drawRoundedRect(toggle,
                            (cheat.enabled ? onColor : offColor).withAlpha(0.85f * opacity),
                            toggleH * 0.5f);

        const float knobX = cheat.enabled
            ? (toggle.right() - knobSize - 3.f)
            : (toggle.x + 3.f);
        const nxui::Rect knob = {knobX, toggle.y + (toggleH - knobSize) * 0.5f, knobSize, knobSize};
        ren.drawRoundedRect(knob, nxui::Color(1.f, 1.f, 1.f, 0.95f * opacity), knobSize * 0.5f);

        const std::string toggleLabel = cheat.enabled
            ? i18n.tr("dialog.cheats_on", "ON")
            : i18n.tr("dialog.cheats_off", "OFF");
        ren.drawText(toggleLabel, {toggle.x - 34.f, toggle.y + 6.f}, m_smallFont,
                     (cheat.enabled ? onColor : subtle).withAlpha(opacity), 0.65f);
    }

    // Bottom Preview Box
    const nxui::Rect bottomInfo = {body.x + 10.f, list.bottom() + 10.f, body.width - 20.f, body.bottom() - list.bottom() - 10.f};
    ren.drawRoundedRect(bottomInfo, m_theme->panelBase.withAlpha(0.05f * opacity), 12.f);
    ren.drawRoundedRectOutline(bottomInfo, m_theme->panelBorder.withAlpha(0.12f * opacity), 12.f, 1.f);

    if (m_selected >= 0 && m_selected < static_cast<int>(cheats.size())) {
        const auto& cur = cheats[static_cast<size_t>(m_selected)];
        ren.drawText(i18n.tr("dialog.cheats_preview", "Opcode Preview") + ":",
                     {bottomInfo.x + 16.f, bottomInfo.y + 10.f}, m_smallFont, subtle, 0.58f);

        std::string preview;
        const size_t maxPreviewLines = 3;
        for (size_t l = 0; l < std::min(maxPreviewLines, cur.lines.size()); ++l) {
            preview += cur.lines[l];
            if (l + 1 < std::min(maxPreviewLines, cur.lines.size())) {
                preview += "  |  ";
            }
        }
        if (cur.lines.size() > maxPreviewLines) {
            preview += " ...";
        }
        if (preview.empty()) {
            preview = "—";
        }
        ren.drawText(preview, {bottomInfo.x + 16.f, bottomInfo.y + 34.f}, m_smallFont, secondary, 0.64f);
    }
}
