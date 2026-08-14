#include "GameModsScreen.hpp"

#include <nxui/core/I18n.hpp>
#include <nxui/core/Renderer.hpp>

#include <algorithm>

GameModsScreen::GameModsScreen()
    : TabbedOverlayScreen(ScreenMode::ThemeShop) {
}

void GameModsScreen::openForGame(std::uint64_t titleId, std::string title) {
    m_titleId = titleId;
    m_title = std::move(title);
    m_selected = 0;
    reload();
    show();
    m_focusArea = FocusArea::Content;
}

void GameModsScreen::buildTabs() {
    m_tabs.clear();
    m_tabs.push_back({"", {}});
}

void GameModsScreen::reload() {
    const std::string keep = selectedName();
    m_entries = mods::GameModManager::scan(m_titleId);
    if (!keep.empty()) {
        for (int i = 0; i < (int)m_entries.size(); ++i) {
            if (m_entries[(size_t)i].name == keep) {
                m_selected = i;
                return;
            }
        }
    }
    m_selected = std::clamp(m_selected, 0, std::max(0, (int)m_entries.size() - 1));
}

std::string GameModsScreen::selectedName() const {
    return (m_selected >= 0 && m_selected < (int)m_entries.size())
        ? m_entries[(size_t)m_selected].name : std::string{};
}

std::string GameModsScreen::kindLabel(mods::Kind kind) const {
    auto& i18n = nxui::I18n::instance();
    switch (kind) {
        case mods::Kind::Romfs: return i18n.tr("dialog.mods_kind_romfs", "RomFS content");
        case mods::Kind::Exefs: return i18n.tr("dialog.mods_kind_exefs", "ExeFS patch");
        case mods::Kind::Cheats: return i18n.tr("dialog.mods_kind_cheats", "Cheat");
    }
    return {};
}

void GameModsScreen::toggleSelected() {
    if (m_entries.empty()) return;
    const auto result = mods::GameModManager::toggle(m_entries[(size_t)m_selected]);
    auto& i18n = nxui::I18n::instance();
    if (result.ok) {
        const bool wasEnabled = m_entries[(size_t)m_selected].enabled;
        reload();
        requestToast(i18n.tr(wasEnabled ? "dialog.mods_disabled_toast" : "dialog.mods_enabled_toast",
                              wasEnabled ? "Mod disabled. Restart the game to apply it."
                                         : "Mod enabled. Restart the game to apply it."));
        if (m_activateSfxCb) m_activateSfxCb();
    } else {
        requestToast(i18n.tr("dialog.mods_operation_failed", "Could not change this mod.") + ": " + result.error);
    }
}

void GameModsScreen::removeSelected() {
    if (m_entries.empty()) return;
    const auto result = mods::GameModManager::remove(m_entries[(size_t)m_selected]);
    auto& i18n = nxui::I18n::instance();
    if (result.ok) {
        reload();
        requestToast(i18n.tr("dialog.mods_removed", "Mod removed."));
    } else {
        requestToast(i18n.tr("dialog.mods_operation_failed", "Could not change this mod.") + ": " + result.error);
    }
}

bool GameModsScreen::handleCustomPressA() {
    toggleSelected();
    return true;
}

bool GameModsScreen::handleCustomPressB() {
    hide();
    return true;
}

bool GameModsScreen::handleCustomPressX() {
    if (m_entries.empty()) return true;
    if (m_removeRequestCb) m_removeRequestCb();
    return true;
}

bool GameModsScreen::handleCustomNavUp() {
    if (!m_entries.empty()) {
        m_selected = (m_selected - 1 + (int)m_entries.size()) % (int)m_entries.size();
        if (m_navSfxCb) m_navSfxCb();
    }
    return true;
}

bool GameModsScreen::handleCustomNavDown() {
    if (!m_entries.empty()) {
        m_selected = (m_selected + 1) % (int)m_entries.size();
        if (m_navSfxCb) m_navSfxCb();
    }
    return true;
}

std::string GameModsScreen::currentAccessibilitySummary() const {
    auto& i18n = nxui::I18n::instance();
    if (m_entries.empty()) return i18n.tr("dialog.mods_empty", "No mods were found for this game.");
    const auto& entry = m_entries[(size_t)m_selected];
    return entry.name + ". " + kindLabel(entry.kind) + ". " +
        i18n.tr(entry.enabled ? "dialog.mods_active" : "dialog.mods_disabled",
                entry.enabled ? "Enabled" : "Disabled");
}

void GameModsScreen::drawCustomContent(nxui::Renderer& ren, const nxui::Rect& panel,
                                       const nxui::Rect&, float opacity) {
    if (!m_theme || !m_font || !m_smallFont) return;
    auto& i18n = nxui::I18n::instance();
    const auto primary = m_theme->textPrimary.withAlpha(opacity);
    const auto secondary = m_theme->textSecondary.withAlpha(0.82f * opacity);
    const auto subtle = m_theme->textSecondary.withAlpha(0.58f * opacity);
    const nxui::Rect body = panel.shrunk(28.f);

    ren.drawText(i18n.tr("dialog.mods_title", "Mods") + " — " + m_title,
                 {body.x + 12.f, body.y + 12.f}, m_font, primary, 1.02f);
    ren.drawText(i18n.tr("dialog.mods_hint", "A enable/disable  •  X remove  •  B back"),
                 {body.x + 14.f, body.y + 48.f}, m_smallFont, secondary, 0.70f);

    const nxui::Rect list = {body.x + 10.f, body.y + 82.f, body.width - 20.f, body.height - 140.f};
    ren.drawRoundedRect(list, m_theme->panelBase.withAlpha(0.07f * opacity), 18.f);
    ren.drawRoundedRectOutline(list, m_theme->panelBorder.withAlpha(0.16f * opacity), 18.f, 1.f);
    if (m_entries.empty()) {
        ren.drawText(i18n.tr("dialog.mods_empty", "No mods were found for this game."),
                     {list.x + 24.f, list.y + 32.f}, m_font, secondary, 0.82f);
        return;
    }

    const int visible = 6;
    const int start = std::clamp(m_selected - visible / 2, 0, std::max(0, (int)m_entries.size() - visible));
    const int end = std::min((int)m_entries.size(), start + visible);
    const float rowH = 74.f;
    for (int i = start; i < end; ++i) {
        const auto& entry = m_entries[(size_t)i];
        const nxui::Rect row = {list.x + 16.f, list.y + 16.f + (i - start) * (rowH + 8.f),
                                list.width - 32.f, rowH};
        const bool selected = i == m_selected;
        ren.drawRoundedRect(row, m_theme->panelBase.withAlpha((selected ? 0.20f : 0.055f) * opacity), 13.f);
        ren.drawRoundedRectOutline(row, (selected ? m_theme->cursorNormal : m_theme->panelBorder)
                                  .withAlpha((selected ? 0.78f : 0.14f) * opacity), 13.f,
                                  selected ? 2.f : 1.f);
        ren.drawText(entry.name, {row.x + 18.f, row.y + 13.f}, m_font, primary, 0.78f);
        ren.drawText(kindLabel(entry.kind), {row.x + 18.f, row.y + 43.f}, m_smallFont, subtle, 0.63f);

        // Match the system-style toggles used by Wi-Fi and Airplane Mode. The
        // visual state is deliberately self-contained: a mod row reads as a
        // controllable setting instead of a label that looks like an action.
        constexpr float toggleW = 64.f;
        constexpr float toggleH = 32.f;
        constexpr float toggleInset = 4.f;
        constexpr float knobSize = 24.f;
        const nxui::Rect toggle = {row.right() - toggleW - 18.f,
                                   row.y + (row.height - toggleH) * 0.5f,
                                   toggleW, toggleH};
        const nxui::Color offColor(0.45f, 0.45f, 0.50f, 1.f);
        const nxui::Color onColor(0.20f, 0.80f, 0.40f, 1.f);
        const auto trackColor = entry.enabled ? onColor : offColor;
        ren.drawRoundedRect(toggle, trackColor.withAlpha(0.90f * opacity), toggleH * 0.5f);
        const float travel = toggleW - toggleInset * 2.f - knobSize;
        const nxui::Rect knob = {toggle.x + toggleInset + (entry.enabled ? travel : 0.f),
                                 toggle.y + toggleInset, knobSize, knobSize};
        ren.drawRoundedRect(knob, nxui::Color::white().withAlpha(opacity), knobSize * 0.5f);
        if (selected) m_focusCursor.moveTo(row, 12.f, 0.08f);
    }
}
