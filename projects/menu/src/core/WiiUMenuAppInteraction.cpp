#include "WiiUMenuApp.hpp"
#include <fmt/format.h>
#include "widgets/GlossyIcon.hpp"
#include "DebugLog.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <nxui/core/I18n.hpp>

namespace {

std::string titleIdHex(std::uint64_t titleId) {
    return fmt::format("{:016X}", titleId);
}

std::string installedDisplayVersion(std::uint64_t titleId) {
#ifdef SWITCHU_MENU
    NsApplicationControlData control{};
    u64 actualSize = 0;
    const Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId,
                                                  &control, sizeof(control), &actualSize);
    if (R_SUCCEEDED(rc) && control.nacp.display_version[0] != '\0') {
        DebugLog::log("[details] version title=%016llX value=%s", (unsigned long long)titleId,
                      control.nacp.display_version);
        return control.nacp.display_version;
    }
    DebugLog::log("[details] version unavailable title=%016llX rc=0x%08X size=%llu",
                  (unsigned long long)titleId, (unsigned int)rc, (unsigned long long)actualSize);
#else
    (void)titleId;
#endif
    return {};
}

std::string installedModSummary(std::uint64_t titleId) {
    const std::filesystem::path base = std::filesystem::path("sdmc:/atmosphere/contents") / titleIdHex(titleId);
    std::error_code ec;
    int count = 0;
    for (const char* directory : {"romfs", "exefs", "cheats"}) {
        ec.clear();
        if (std::filesystem::is_directory(base / directory, ec) && !ec)
            ++count;
    }
    auto& i18n = nxui::I18n::instance();
    if (count == 0)
        return i18n.tr("dialog.details_mods_none", "None detected");
    DebugLog::log("[details] mods title=%016llX detected=%d", (unsigned long long)titleId, count);
    return i18n.tr("dialog.details_mods_detected", "Detected") + ": " + std::to_string(count);
}

std::string installedPlayTime(std::uint64_t titleId) {
#ifdef SWITCHU_MENU
    PdmPlayStatistics stats{};
    const Result initRc = pdmqryInitialize();
    if (R_SUCCEEDED(initRc)) {
        const Result queryRc = pdmqryQueryPlayStatisticsByApplicationId(titleId, true, &stats);
        pdmqryExit();
        if (R_SUCCEEDED(queryRc) && stats.playtime > 0) {
            const std::uint64_t minutes = stats.playtime / 60000000000ULL;
            if (minutes >= 60)
                return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
            return std::to_string(minutes) + " min";
        }
        DebugLog::log("[details] playtime unavailable title=%016llX init=0x%X query=0x%X",
                      static_cast<unsigned long long>(titleId), initRc, queryRc);
    } else {
        DebugLog::log("[details] pdmqry init failed title=%016llX rc=0x%X",
                      static_cast<unsigned long long>(titleId), initRc);
    }
#else
    (void)titleId;
#endif
    return nxui::I18n::instance().tr("dialog.details_playtime_unavailable", "Not available yet");
}

} // namespace

bool WiiUMenuApp::isEditableIcon(nxui::Widget* w) const {
    if (!w || w->tag() != "glossy_icon")
        return false;
    auto* icon = static_cast<GlossyIcon*>(w);
    return icon->titleId() != 0;
}

std::string WiiUMenuApp::accessibilityContextFor(nxui::Widget* w) const {
    auto& i18n = nxui::I18n::instance();
    if (!w)
        return {};
    if (m_dialog && m_dialog->isActive() && w == m_dialog.get())
        return i18n.tr("accessibility.context.dialog", "Dialog");
    if (m_userSelect && m_userSelect->isActive() && w == m_userSelect.get())
        return i18n.tr("accessibility.context.profile_selection", "Profile selection");
    if (m_settings && m_settings->isActive() && w == m_settings.get())
        return i18n.tr("accessibility.context.settings", "Settings");
    if (m_themeShop && m_themeShop->isActive() && w == m_themeShop.get())
        return i18n.tr("accessibility.context.themes", "Themes");
    if (m_gameGallery && m_gameGallery->isActive() && w == m_gameGallery.get())
        return i18n.tr("dialog.gallery_title", "Gallery");
    if (m_gameMods && m_gameMods->isActive() && w == m_gameMods.get())
        return i18n.tr("dialog.mods_title", "Mods");
    if (m_gameDetails && m_gameDetails->isActive() && w == m_gameDetails.get())
        return i18n.tr("dialog.details_title", "Game details");
    if (w->tag() == "glossy_icon" && m_grid)
        return i18n.tr("accessibility.context.main_menu", "Main menu")
             + ", " + i18n.tr("accessibility.context.page", "page") + " "
             + std::to_string(m_grid->currentPage() + 1)
             + " " + i18n.tr("accessibility.context.of", "of") + " "
             + std::to_string(m_grid->totalPages());
    for (const auto& btn : m_sidebar.leftButtons())
        if (btn.get() == w) return i18n.tr("accessibility.context.left_sidebar", "Left sidebar");
    for (const auto& btn : m_sidebar.rightButtons())
        if (btn.get() == w) return i18n.tr("accessibility.context.right_sidebar", "Right sidebar");
    for (const auto& avatar : m_userAvatarButtons)
        if (avatar.get() == w) return i18n.tr("accessibility.context.user_profiles", "User profiles");
    return {};
}

std::string WiiUMenuApp::accessibilityActionsFor(nxui::Widget* w) const {
    auto& i18n = nxui::I18n::instance();
    if (!w)
        return {};
    if (m_editMode && w->tag() == "glossy_icon")
        return i18n.tr("accessibility.actions.edit_mode", "Directional pad to choose the new position. Y to place. B to cancel.");
    if (w->tag() == "glossy_icon") {
        auto* icon = static_cast<GlossyIcon*>(w);
        if (icon->titleId() == 0)
            return i18n.tr("accessibility.actions.empty_slot", "Directional pad to navigate. ZL or ZR to change page.");
        return icon->isNotLaunchable()
            ? i18n.tr("accessibility.actions.game_blocked", "A to show the reason. Y to move. ZL or ZR to change page.")
            : i18n.tr("accessibility.actions.game_launchable", "A to launch. X for options. Y to move. ZL or ZR to change page.");
    }
    if (m_settings && w == m_settings.get())
        return i18n.tr("accessibility.actions.settings", "Up and down to choose a category. A or right to enter. B to close.");
    if (m_themeShop && w == m_themeShop.get())
        return i18n.tr("accessibility.actions.themes", "Up and down to navigate. A to choose. B to close.");
    if (m_gameGallery && w == m_gameGallery.get())
        return i18n.tr("accessibility.actions.themes", "Up and down to navigate. A to choose. B to close.");
    if (m_gameMods && w == m_gameMods.get())
        return i18n.tr("dialog.mods_hint", "A enable/disable. X remove. B back.");
    if (m_gameDetails && w == m_gameDetails.get())
        return i18n.tr("dialog.details_actions", "Left and right to select gameplay art. A to expand. B to return.");
    if ((m_dialog && w == m_dialog.get()) || (m_userSelect && w == m_userSelect.get()))
        return i18n.tr("accessibility.actions.dialog", "Left and right to change choice. A to confirm. B to cancel.");
    return {};
}

std::string WiiUMenuApp::accessibilityPositionFor(nxui::Widget* w) const {
    if (!w || !m_config.accessibilitySpeakPosition)
        return {};

    auto& i18n = nxui::I18n::instance();
    if (w->tag() == "glossy_icon" && m_grid) {
        const int global = m_grid->focusedGlobalIndex();
        if (global >= 0) {
            const int cols = std::max(1, m_grid->columns());
            const int rows = std::max(1, m_grid->rowsPerPage());
            const int local = global % std::max(1, m_grid->iconsPerPage());
            const int row = local / cols + 1;
            const int col = local % cols + 1;
            return i18n.tr("accessibility.position.row", "row") + " " + std::to_string(row)
                 + " " + i18n.tr("accessibility.context.of", "of") + " " + std::to_string(rows)
                 + ". " + i18n.tr("accessibility.position.column", "column") + " " + std::to_string(col)
                 + " " + i18n.tr("accessibility.context.of", "of") + " " + std::to_string(cols);
        }
    }

    auto describeLinear = [&](const auto& buttons) -> std::string {
        for (int i = 0; i < (int)buttons.size(); ++i) {
            if (buttons[(size_t)i].get() == w) {
                return std::to_string(i + 1) + " "
                     + i18n.tr("accessibility.context.of", "of") + " "
                     + std::to_string((int)buttons.size());
            }
        }
        return {};
    };

    if (auto text = describeLinear(m_sidebar.leftButtons()); !text.empty())
        return text;
    if (auto text = describeLinear(m_sidebar.rightButtons()); !text.empty())
        return text;

    for (int i = 0; i < (int)m_userAvatarButtons.size(); ++i) {
        if (m_userAvatarButtons[(size_t)i].get() == w) {
            return std::to_string(i + 1) + " "
                 + i18n.tr("accessibility.context.of", "of") + " "
                 + std::to_string((int)m_userAvatarButtons.size());
        }
    }

    return {};
}

void WiiUMenuApp::announceFocusedWidget(nxui::Widget* w) {
    if (!w)
        return;

    std::string hint = accessibilityActionsFor(w);
    std::string position = accessibilityPositionFor(w);

    std::string originalHint = w->accessibilityHint();
    if (m_accessibility.speakHints()) {
        if (!hint.empty())
            w->setAccessibilityHint(hint);
    } else {
        w->setAccessibilityHint({});
    }
    const bool forceRepeat = w->tag() == "glossy_icon";
    std::string summary = w->accessibilitySummary();
    if (summary.empty())
        summary = w->tag();
    m_accessibility.announceStructuredFocus(accessibilityContextFor(w),
                                            position,
                                            summary,
                                            forceRepeat);
    if ((m_accessibility.speakHints() && !hint.empty()) || !m_accessibility.speakHints())
        w->setAccessibilityHint(originalHint);
}

void WiiUMenuApp::startEditGhost(GlossyIcon* sourceIcon) {
    stopEditGhost();
    if (!sourceIcon)
        return;

    if (m_editSourceIndex >= 0)
        m_iconStreamer.setPinnedIndex(m_editSourceIndex);

    m_editSourceIcon = sourceIcon;
    m_editSourceIcon->setOpacity(0.10f);

    auto ghost = std::make_shared<GlossyIcon>();
    ghost->setTag("edit_ghost");
    ghost->setFocusable(false);
    ghost->setTitle(sourceIcon->title());
    ghost->setTitleId(sourceIcon->titleId());
    ghost->setTexture(sourceIcon->texture());
    ghost->setIsGameCard(sourceIcon->isGameCard());
    ghost->setGameCardTexture(sourceIcon->gameCardTexture());
    ghost->setNotLaunchable(sourceIcon->isNotLaunchable());
    ghost->setCornerRadius(sourceIcon->cornerRadius());
    ghost->setBlurEnabled(false);
    ghost->setPanelOpacity(0.84f);
    ghost->setOpacity(0.84f);
    ghost->setScale(1.06f);
    ghost->forceVisible();

    m_editGhostTargetRect = sourceIcon->focusRect().expanded(4.f);
    ghost->setRect(m_editGhostTargetRect);
    m_editGhostPulse = 0.f;

    m_editGhostIcon = ghost;
}

void WiiUMenuApp::stopEditGhost() {
    m_iconStreamer.clearPinnedIndex();

    if (m_editSourceIcon)
        m_editSourceIcon->setOpacity(1.f);
    m_editSourceIcon = nullptr;

    m_editGhostIcon.reset();
    m_editGhostPulse = 0.f;
}

void WiiUMenuApp::updateEditGhost(float dt) {
    if (!m_editMode || !m_editGhostIcon)
        return;

    if (m_cursor && m_cursor->isVisible()) {
        m_editGhostTargetRect = m_cursor->currentRect();
    } else if (auto* cur = focusManager().current()) {
        if (cur->tag() == "glossy_icon")
            m_editGhostTargetRect = cur->focusRect().expanded(4.f);
    }

    m_editGhostPulse += dt;
    float pulse = 0.80f + 0.08f * std::sin(m_editGhostPulse * 8.f);
    m_editGhostIcon->setOpacity(pulse);
    m_editGhostIcon->setPanelOpacity(std::min(1.f, pulse + 0.12f));
    m_editGhostIcon->setScale(1.07f + 0.025f * std::sin(m_editGhostPulse * 7.f));

    m_editGhostIcon->setRect(m_editGhostTargetRect);
}

void WiiUMenuApp::unbindEditActions() {
    if (!m_editBoundIcon)
        return;
    m_editBoundIcon->clearActions();
    m_editBoundIcon = nullptr;
}

void WiiUMenuApp::bindEditActions(GlossyIcon* icon) {
    if (!icon)
        return;
    if (m_editBoundIcon == icon)
        return;

    unbindEditActions();
    m_editBoundIcon = icon;

    icon->addAction(static_cast<uint64_t>(nxui::Button::A), []() {});
    icon->addAction(static_cast<uint64_t>(nxui::Button::B), [this]() {
        exitEditMode();
        m_audio.playSfx(Sfx::ModalHide);
    });
}

void WiiUMenuApp::enterEditMode() {
    auto* cur = focusManager().current();
    if (!isEditableIcon(cur))
        return;

    auto* icon = static_cast<GlossyIcon*>(cur);
    m_editMode = true;
    m_editSourceIndex = m_grid ? m_grid->focusedGlobalIndex() : -1;
    m_editHeldTitle = icon->title();
    startEditGhost(icon);
    bindEditActions(icon);
    m_titlePill->setText(nxui::I18n::instance().tr("game.move_prefix", "Move: ") + m_editHeldTitle);
    m_titlePill->setVisible(true);
    m_accessibility.announce(nxui::I18n::instance().tr(
        "accessibility.move_mode.enter",
        "Moving game: ") + m_editHeldTitle, true, true);
}

void WiiUMenuApp::exitEditMode() {
    if (!m_editMode)
        return;

    m_editMode = false;
    unbindEditActions();
    m_editSourceIndex = -1;
    m_editHeldTitle.clear();
    stopEditGhost();

    auto* cur = focusManager().current();
    if (isEditableIcon(cur)) {
        auto* icon = static_cast<GlossyIcon*>(cur);
        m_titlePill->setText(icon->title());
        m_titlePill->setVisible(true);
    } else {
        m_titlePill->hideAnimated();
    }

    if (m_layoutDirty)
        saveMenuLayout();
}

bool WiiUMenuApp::commitEditModePlacement() {
    if (!m_editMode || !m_grid)
        return false;

    int from = m_editSourceIndex;
    int target = m_grid->focusedGlobalIndex();
    if (from < 0 || target < 0 || from >= m_model.count() || target >= m_model.count())
        return false;
    if (m_model.at(from).titleId == 0)
        return false;

    int oldPage = m_grid->currentPage();
    bool changed = (from != target);
    if (changed) {
        if (!m_layoutSlots.empty() && from < (int)m_layoutSlots.size() && target < (int)m_layoutSlots.size())
            std::swap(m_layoutSlots[from], m_layoutSlots[target]);

        m_model.swapEntries(from, target);
        m_iconStreamer.swapIndices(from, target);
        m_grid->swapSlots(from, target);
        m_editSourceIndex = target;
        m_iconStreamer.setPinnedIndex(m_editSourceIndex);
        m_layoutDirty = true;
    }

    m_grid->focusGlobalIndex(target);

    int newPage = m_grid->currentPage();
    if (changed || newPage != oldPage) {
        m_iconStreamer.onPageChanged(newPage, m_grid->iconsPerPage(),
                                     app().gpu(), app().renderer(),
                                     m_grid->allIcons());
    }
    for (auto* icon : m_grid->pageIcons()) {
        if (icon)
            icon->forceVisible();
    }

    if (auto* cur = m_grid->focusManager().current())
        focusManager().setFocus(cur);

    if (m_editGhostIcon) {
        if (auto* focused = m_grid->focusManager().current())
            m_editGhostTargetRect = focused->focusRect().expanded(4.f);
    }

    updateCursor();
    return true;
}

bool WiiUMenuApp::moveFocusedIcon(nxui::FocusDirection dir) {
    if (!m_editMode || !m_grid)
        return false;

    int from = m_grid->focusedGlobalIndex();
    if (from < 0 || from >= m_model.count())
        return false;
    if (m_model.at(from).titleId == 0)
        return false;

    int cols = std::max(1, m_grid->columns());
    int rows = std::max(1, m_grid->rowsPerPage());
    int perPage = std::max(1, m_grid->iconsPerPage());
    int totalPages = std::max(1, m_grid->totalPages());

    int page = from / perPage;
    int local = from % perPage;
    int col = local % cols;
    int row = local / cols;

    int target = from;
    switch (dir) {
        case nxui::FocusDirection::LEFT:
            if (col > 0)
                target = from - 1;
            else if (page > 0)
                target = (page - 1) * perPage + row * cols + (cols - 1);
            else
                return false;
            break;
        case nxui::FocusDirection::RIGHT:
            if (col < cols - 1)
                target = from + 1;
            else if (page + 1 < totalPages)
                target = (page + 1) * perPage + row * cols;
            else
                return false;
            break;
        case nxui::FocusDirection::UP:
            if (row > 0)
                target = from - cols;
            else
                return false;
            break;
        case nxui::FocusDirection::DOWN:
            if (row + 1 < rows)
                target = from + cols;
            else
                return false;
            break;
    }

    if (target < 0 || target >= m_model.count())
        return false;
    if (target == from)
        return true;

    if (!m_layoutSlots.empty() && from < (int)m_layoutSlots.size() && target < (int)m_layoutSlots.size())
        std::swap(m_layoutSlots[from], m_layoutSlots[target]);

    m_model.swapEntries(from, target);
    m_iconStreamer.swapIndices(from, target);
    m_grid->swapSlots(from, target);
    m_editSourceIndex = target;
    m_iconStreamer.setPinnedIndex(m_editSourceIndex);
    m_grid->focusGlobalIndex(target);

    int newPage = m_grid->currentPage();
    m_iconStreamer.onPageChanged(newPage, m_grid->iconsPerPage(),
                                 app().gpu(), app().renderer(),
                                 m_grid->allIcons());
    for (auto* icon : m_grid->pageIcons()) {
        if (icon)
            icon->forceVisible();
    }

    if (auto* cur = m_grid->focusManager().current())
        focusManager().setFocus(cur);

    auto* cur = focusManager().current();
    if (isEditableIcon(cur)) {
        auto* icon = static_cast<GlossyIcon*>(cur);
        bindEditActions(icon);
        m_titlePill->setText(nxui::I18n::instance().tr("game.move_prefix", "Move: ") + icon->title());
    }

    m_layoutDirty = true;
    updateCursor();
    return true;
}

void WiiUMenuApp::wireFocusCallback() {
    focusManager().onFocusChanged([this](nxui::Widget*, nxui::Widget* cur) {
        updateCursor();
        announceFocusedWidget(cur);

        if ((m_dialog && m_dialog->isActive()) ||
            (m_themeShop && m_themeShop->isActive()) ||
            (m_gameGallery && m_gameGallery->isActive()) ||
            (m_gameMods && m_gameMods->isActive()) ||
            (m_gameDetails && m_gameDetails->isActive()) ||
            (m_settings && m_settings->isActive()) ||
            (m_userSelect && m_userSelect->isActive()))
            return;

        bool suppressSfx = m_suppressNextNavigateSfx;
        m_suppressNextNavigateSfx = false;
        if (!suppressSfx)
            m_audio.playSfx(Sfx::Navigate);

        if (cur && cur->tag() == "glossy_icon") {
            m_grid->focusManager().setFocus(cur);
            auto* icon = static_cast<GlossyIcon*>(cur);
            auto& i18n = nxui::I18n::instance();
            if (m_editMode) {
                bindEditActions(icon);
                m_editGhostTargetRect = icon->focusRect();
                if (!m_editHeldTitle.empty())
                    m_titlePill->setText(i18n.tr("game.move_prefix", "Move: ") + m_editHeldTitle);
                else if (icon->titleId() != 0)
                    m_titlePill->setText(i18n.tr("game.move_prefix", "Move: ") + icon->title());
                else
                    m_titlePill->setText(i18n.tr("game.move", "Move"));
                m_titlePill->setVisible(true);
                return;
            }
            if (icon->titleId() == 0) {
                refreshGameArtworkBackdrop(0);
                m_titlePill->hideAnimated();
                return;
            }
            refreshGameArtworkBackdrop(icon->titleId());
#ifdef SWITCHU_MENU
            if (m_launcher.isAppSuspended(icon->titleId())) {
                m_titlePill->setText(icon->title());
            } else
#endif
            m_titlePill->setText(icon->title());
            m_titlePill->setVisible(true);
        } else if (cur) {
            refreshGameArtworkBackdrop(0);
            if (m_editMode)
                exitEditMode();
            for (auto& btn : m_sidebar.leftButtons()) {
                if (btn.get() == cur) { m_titlePill->setText(btn->label()); m_titlePill->setVisible(true); return; }
            }
            for (auto& btn : m_sidebar.rightButtons()) {
                if (btn.get() == cur) { m_titlePill->setText(btn->label()); m_titlePill->setVisible(true); return; }
            }
            for (auto& avatar : m_userAvatarButtons) {
                if (avatar.get() == cur) {
                    m_titlePill->setText(avatar->nickname());
                    m_titlePill->setVisible(!avatar->nickname().empty());
                    return;
                }
            }
            m_titlePill->hideAnimated();
        } else {
            refreshGameArtworkBackdrop(0);
            m_titlePill->hideAnimated();
        }
    });
    updateCursor();
    if (auto* cur = focusManager().current()) {
        if (cur->tag() == "glossy_icon") {
            auto* icon = static_cast<GlossyIcon*>(cur);
            if (icon->titleId() != 0)
                m_titlePill->setText(icon->title());
            refreshGameArtworkBackdrop(icon->titleId());
        }
    }
}

void WiiUMenuApp::refreshGameArtworkBackdrop(std::uint64_t titleId) {
    if (!m_gameArtworkBackdrop)
        return;
    const std::string path = gallery::GameArtworkStore::pathFor(
        titleId, gallery::ArtworkKind::Background);
    if (path == m_gameArtworkBackdrop->artworkPath())
        return;
    if (path.empty()) {
        m_gameArtworkBackdrop->clearArtwork();
        return;
    }
    if (m_gameArtworkBackdrop->setArtwork(app().gpu(), app().renderer(), path)) {
        DebugLog::log("[gallery] activated background: %s", path.c_str());
    } else {
        DebugLog::log("[gallery] could not load custom background: %s", path.c_str());
        m_gameArtworkBackdrop->clearArtwork();
    }
}

bool WiiUMenuApp::isCurrentFocusableWidget(nxui::Widget* w) const {
    if (!w) return false;
    if (m_gameGallery && m_gameGallery.get() == w) return w->isFocusable();
    if (m_gameMods && m_gameMods.get() == w) return w->isFocusable();
    if (m_gameDetails && m_gameDetails.get() == w) return w->isFocusable();
    if (m_themeShop && m_themeShop.get() == w) return w->isFocusable();
    if (m_settings && m_settings.get() == w) return w->isFocusable();
    for (const auto& btn : m_sidebar.leftButtons())
        if (btn.get() == w) return w->isFocusable();
    for (const auto& btn : m_sidebar.rightButtons())
        if (btn.get() == w) return w->isFocusable();
    for (const auto& avatar : m_userAvatarButtons)
        if (avatar.get() == w) return w->isFocusable();
    if (m_grid)
        for (const auto& icon : m_grid->allIcons())
            if (icon.get() == w) return w->isFocusable();
    return false;
}

int WiiUMenuApp::findTitleIndex(uint64_t titleId) const {
    if (titleId == 0)
        return -1;
    for (int i = 0; i < m_model.count(); ++i) {
        if (m_model.at(i).titleId == titleId)
            return i;
    }
    return -1;
}

bool WiiUMenuApp::focusTitle(uint64_t titleId) {
    if (!m_grid)
        return false;

    int idx = findTitleIndex(titleId);
    if (idx < 0)
        return false;

    int oldPage = m_grid->currentPage();
    if (!m_grid->focusGlobalIndex(idx))
        return false;

    if (m_grid->currentPage() != oldPage || titleId != 0) {
        m_iconStreamer.onPageChanged(m_grid->currentPage(), m_grid->iconsPerPage(),
                                     app().gpu(), app().renderer(),
                                     m_grid->allIcons());
    }

    if (auto* cur = m_grid->focusManager().current())
        focusManager().setFocus(cur);
    updateCursor();
    return true;
}

void WiiUMenuApp::markSuspendedIcon(uint64_t titleId) {
    if (!m_grid)
        return;
    for (auto& icon : m_grid->allIcons())
        icon->setSuspended(titleId != 0 && icon->titleId() == titleId);
    if (titleId != 0)
        focusTitle(titleId);

    if (auto* cur = m_grid->focusManager().current()) {
        auto* icon = static_cast<GlossyIcon*>(cur);
        if (m_launcher.isAppSuspended(icon->titleId())) {
            m_titlePill->setText(icon->title());
        } else {
            m_titlePill->setText(icon->title());
        }
    }
}

void WiiUMenuApp::closeActiveOverlays() {
    if (m_editMode)
        exitEditMode();
    if (m_userSelect && m_userSelect->isActive())
        m_userSelect->hide();
    if (m_dialog && m_dialog->isActive())
        m_dialog->hide();
    if (m_settings && m_settings->isActive())
        m_settings->hide();
    if (m_themeShop && m_themeShop->isActive())
        m_themeShop->hide();
    if (m_gameGallery && m_gameGallery->isActive())
        m_gameGallery->hide();
    if (m_gameMods && m_gameMods->isActive())
        m_gameMods->hide();
    if (m_gameDetails && m_gameDetails->isActive())
        m_gameDetails->hide();
}

nxui::Widget* WiiUMenuApp::focusRoot() {
    if (m_launchAnim && m_launchAnim->isPlaying()) return nullptr;
    if (m_dialog && m_dialog->isActive()) return m_dialog.get();
    if (m_gameMods && m_gameMods->isActive()) return m_gameMods.get();
    if (m_gameGallery && m_gameGallery->isActive()) return m_gameGallery.get();
    if (m_gameDetails && m_gameDetails->isActive()) return m_gameDetails.get();
    if (m_themeShop && m_themeShop->isActive()) return m_themeShop.get();
    if (m_settings && m_settings->isActive()) return m_settings.get();
    if (m_userSelect && m_userSelect->isActive()) return m_userSelect.get();
    return &rootBox();
}

void WiiUMenuApp::toggleAccessibilitySpeech() {
    auto& i18n = nxui::I18n::instance();
    const bool enabled = !m_config.accessibilityEnabled;
    m_config.accessibilityEnabled = enabled;
    if (m_settings)
        m_settings->setAccessibilityEnabledState(enabled);
    if (m_themeShop)
        m_themeShop->setAccessibilityVoiceEnabled(enabled);
    if (m_gameGallery)
        m_gameGallery->setAccessibilityVoiceEnabled(enabled);
    if (m_gameMods)
        m_gameMods->setAccessibilityVoiceEnabled(enabled);
    if (m_gameDetails)
        m_gameDetails->setAccessibilityVoiceEnabled(enabled);

    if (enabled) {
        m_audio.playSfx(Sfx::ThemeToggle);
        m_accessibility.setEnabled(true);
        m_accessibility.announce(i18n.tr("accessibility.speech.enabled",
                                         "Voice guidance enabled."), true, true);
    } else {
        m_accessibility.announceAndDisable(i18n.tr("accessibility.speech.disabled",
                                                   "Voice guidance disabled."));
    }
    m_config.save();
}

void WiiUMenuApp::handleSortShortcutRelease(float dt) {
    auto& input = app().input();
    if (input.isDown(nxui::Button::R)) {
        m_sortShortcutArmed = true;
        m_sortShortcutHeld = 0.f;
    } else if (m_sortShortcutArmed && input.isHeld(nxui::Button::R)) {
        m_sortShortcutHeld += dt;
    }
    if (!input.isUp(nxui::Button::R))
        return;
    const bool armed = m_sortShortcutArmed;
    const float held = m_sortShortcutHeld;
    m_sortShortcutArmed = false;
    m_sortShortcutHeld = 0.f;
    if (!armed)
        return;
    // Only a tap sorts. Holding R is how the homebrew override reaches Sphaira,
    // and a player doing that is not asking for the grid to be reordered when
    // they let go -- which is what made the shortcut feel like it fired twice.
    if (held > kSortShortcutTapSeconds)
        return;
    // Anything with its own R meaning, or any overlay covering the grid, keeps
    // the shortcut out of the way: the sort belongs to the grid alone.
    if ((m_dialog && m_dialog->isActive()) ||
        (m_themeShop && m_themeShop->isActive()) ||
        (m_gameGallery && m_gameGallery->isActive()) ||
        (m_gameMods && m_gameMods->isActive()) ||
        (m_gameDetails && m_gameDetails->isActive()) ||
        (m_settings && m_settings->isActive()) ||
        (m_userSelect && m_userSelect->isActive()) ||
        m_editMode) {
        return;
    }
    cycleSortMode();
}

bool WiiUMenuApp::handleAccessibilityToggleCombo() {
    auto& input = app().input();
    if (!input.isDown(nxui::Button::Plus) || !input.isDown(nxui::Button::Minus))
        return false;
    if (m_accessibilityToggleComboHeld)
        return true;
    m_accessibilityToggleComboHeld = true;
    m_plusExitPending = false;
    m_plusExitPendingTimer = 0.f;
    toggleAccessibilitySpeech();
    return true;
}

void WiiUMenuApp::wireGlobalActions() {
    auto& root = rootBox();

    root.addAction(static_cast<uint64_t>(nxui::Button::L), [this]() {
        m_accessibility.repeatLastAnnouncement();
    });

    // R is deliberately not wired as a press action. Holding it over a game is
    // how the homebrew override reaches Sphaira, and reordering the grid on the
    // initial press moved the tiles out from under the player mid-hold. The
    // sort now advances on release instead; see handleSortShortcutRelease.
    // The hint panel still advertises R, which is drawn from sortModeLabel().

    root.addAction(static_cast<uint64_t>(nxui::Button::ZL), [this]() {
        int p = m_grid->currentPage() - 1;
        if (p >= 0 && !m_grid->isTransitioning()) {
            m_grid->startWaveTransition(p);
            m_audio.playSfx(Sfx::PageChange);
        }
    });
    root.addAction(static_cast<uint64_t>(nxui::Button::ZR), [this]() {
        int p = m_grid->currentPage() + 1;
        if (p < m_grid->totalPages() && !m_grid->isTransitioning()) {
            m_grid->startWaveTransition(p);
            m_audio.playSfx(Sfx::PageChange);
        }
    });
    root.addAction(static_cast<uint64_t>(nxui::Button::Y), [this]() {
        if ((m_dialog && m_dialog->isActive()) ||
            (m_themeShop && m_themeShop->isActive()) ||
            (m_gameGallery && m_gameGallery->isActive()) ||
            (m_gameMods && m_gameMods->isActive()) ||
            (m_gameDetails && m_gameDetails->isActive()) ||
            (m_settings && m_settings->isActive()) ||
            (m_userSelect && m_userSelect->isActive())) {
            return;
        }

        if (m_editMode) {
            const std::string movedTitle = m_editHeldTitle;
            bool changed = commitEditModePlacement();
            exitEditMode();
            if (!movedTitle.empty()) {
                auto* focused = focusManager().current();
                std::string summary = nxui::I18n::instance().tr(
                    changed ? "accessibility.move_mode.placed" : "accessibility.move_mode.cancelled",
                    changed ? "Game moved: " : "Move cancelled: ") + movedTitle;
                if (focused) {
                    std::string context = accessibilityContextFor(focused);
                    std::string position = accessibilityPositionFor(focused);
                    if (!position.empty())
                        context = context.empty() ? position : context + ". " + position;
                    if (!context.empty())
                        summary += ". " + context;
                    if (!focused->accessibilitySummary().empty())
                        summary += ". " + focused->accessibilitySummary();
                }
                m_accessibility.announce(summary, true, true);
            }
            m_audio.playSfx(changed ? Sfx::ConfirmPositive : Sfx::ModalHide);
            return;
        }

        auto* cur = focusManager().current();
        if (!isEditableIcon(cur))
            return;

        enterEditMode();
        m_audio.playSfx(Sfx::Activate);
    });
#ifdef SWITCHU_DEBUG_UI
    root.addAction(static_cast<uint64_t>(nxui::Button::Minus), [this]() {
        if (handleAccessibilityToggleCombo())
            return;
        m_showDebugOverlay = !m_showDebugOverlay;
        DebugLog::log("[debug] ImGui overlay toggled: %d", m_showDebugOverlay ? 1 : 0);
    });
#else
    root.addAction(static_cast<uint64_t>(nxui::Button::Minus), [this]() {
        handleAccessibilityToggleCombo();
    });
#endif
#ifdef SWITCHU_HOMEBREW
    root.addAction(static_cast<uint64_t>(nxui::Button::Plus), [this]() {
        if (handleAccessibilityToggleCombo())
            return;
        m_plusExitPending = true;
        m_plusExitPendingTimer = 0.80f;
    });
#else
    root.addAction(static_cast<uint64_t>(nxui::Button::Plus), [this]() {
        if (handleAccessibilityToggleCombo())
            return;
        showIconOptions();
    });
#endif

#ifdef SWITCHU_MENU
    root.addAction(static_cast<uint64_t>(nxui::Button::X), [this]() {
        if (m_editMode) return;
        if (m_launcher.suspendedTitleId() == 0) return;
        auto* cur = focusManager().current();
        if (!cur || cur->tag() != "glossy_icon") return;
        auto* icon = static_cast<GlossyIcon*>(cur);
        if (!m_launcher.isAppSuspended(icon->titleId())) return;

        m_audio.playSfx(Sfx::ModalShow);
        m_dialogReturnFocus = cur;
        auto& i18n = nxui::I18n::instance();
        m_dialog->show(
            i18n.tr("game.close_title", "Close game"),
            i18n.tr("game.close_prefix", "Close") + std::string(" ") + icon->title()
                + i18n.tr("game.close_suffix", "?\nUnsaved progress will be lost."),
            {
                {i18n.tr("button.cancel", "Cancel"), [this]() {}, true},
                {i18n.tr("button.close", "Close"),  [this]() {
                    m_launcher.terminateApplication();
                    m_launcher.setAppRunning(false);
                    m_launcher.setAppHasForeground(false);
                    m_launcher.setSuspendedTitleId(0);
                    for (auto& ic : m_grid->allIcons())
                        ic->setSuspended(false);
                    if (auto* cur = m_grid->focusManager().current()) {
                        auto* icon = static_cast<GlossyIcon*>(cur);
                        m_titlePill->setText(icon->title());
                    }
                }, true}
            },
            1,
            {}
        );
        focusManager().setFocus(m_dialog.get());
    });
#endif
}

void WiiUMenuApp::handleTouch() {
    constexpr float kSwipeThreshold = 80.f;
    constexpr float kLongPressThreshold = 0.55f;
    constexpr float kLongPressMoveThreshold = 18.f;

    auto& input = app().input();

    auto hitAvatar = [this](float x, float y) -> UserAvatarButton* {
        for (auto& avatar : m_userAvatarButtons) {
            if (avatar && avatar->isVisible() && avatar->hitTest(x, y))
                return avatar.get();
        }
        return nullptr;
    };

    auto focusTouchedIcon = [this](int localHit) -> GlossyIcon* {
        if (!m_grid || localHit < 0)
            return nullptr;

        int global = m_grid->currentPage() * m_grid->iconsPerPage() + localHit;
        if (!m_grid->focusGlobalIndex(global))
            return nullptr;

        auto* cur = m_grid->focusManager().current();
        if (!cur)
            return nullptr;

        focusManager().setFocus(cur);
        if (m_cursor) {
            m_cursor->moveTo(cur->focusRect().expanded(4.f), 0.f);
            m_cursor->setVisible(true);
        } else {
            updateCursor();
        }

        if (!isEditableIcon(cur))
            return nullptr;
        return static_cast<GlossyIcon*>(cur);
    };

    if (input.touchDown()) {
        float tx = input.touchX();
        float ty = input.touchY();
        m_touchAvatarTarget = hitAvatar(tx, ty);
        m_touchAvatarWasFocused = m_touchAvatarTarget && (focusManager().current() == m_touchAvatarTarget);
        if (m_touchAvatarTarget) {
            m_touchHitIndex = -1;
            m_touchOnFocused = false;
            m_touchEditDragActive = false;
            return;
        }

        int hit = m_grid->hitTest(tx, ty);
        m_touchHitIndex = hit;
        m_touchOnFocused = false;
        m_touchEditDragActive = false;
        if (hit >= 0) {
            auto icons = m_grid->pageIcons();
            if (hit < (int)icons.size())
                m_touchOnFocused = (icons[hit] == focusManager().current());
        }
    }

    if (input.isTouching() && m_touchHitIndex >= 0) {
        float dx = input.touchDeltaX();
        float dy = input.touchDeltaY();

        if (!m_editMode
            && std::abs(dx) <= kLongPressMoveThreshold
            && std::abs(dy) <= kLongPressMoveThreshold
            && input.touchDuration() >= kLongPressThreshold)
        {
            if (auto* icon = focusTouchedIcon(m_touchHitIndex)) {
                enterEditMode();
                if (m_editMode) {
                    m_touchEditDragActive = true;
                    m_audio.playSfx(Sfx::Activate);
                    m_editGhostTargetRect = icon->focusRect().expanded(4.f);
                }
            }
        }

        if (m_editMode && m_touchEditDragActive) {
            int dragHit = m_grid->hitTest(input.touchX(), input.touchY());
            if (dragHit >= 0)
                focusTouchedIcon(dragHit);
        }
    }

    if (input.touchUp()) {
        if (m_touchAvatarTarget) {
            float dx = input.touchDeltaX();
            float dy = input.touchDeltaY();
            UserAvatarButton* avatar = m_touchAvatarTarget;
            m_touchAvatarTarget = nullptr;
            if (std::abs(dx) < 20.f && std::abs(dy) < 20.f &&
                hitAvatar(input.touchX(), input.touchY()) == avatar)
            {
                focusManager().setFocus(avatar);
                if (!m_touchAvatarWasFocused)
                    avatar->activate();
            }
            m_touchAvatarWasFocused = false;
            return;
        }

        if (m_editMode && m_touchEditDragActive) {
            bool changed = commitEditModePlacement();
            exitEditMode();
            m_audio.playSfx(changed ? Sfx::ConfirmPositive : Sfx::ModalHide);
            m_touchHitIndex = -1;
            m_touchEditDragActive = false;
            return;
        }

        float dx = input.touchDeltaX();
        float dy = input.touchDeltaY();
        if (std::abs(dx) > kSwipeThreshold && std::abs(dx) > std::abs(dy) * 1.5f) {
            int p = m_grid->currentPage() + (dx < 0 ? 1 : -1);
            if (p >= 0 && p < m_grid->totalPages() && !m_grid->isTransitioning()) {
                m_grid->startWaveTransition(p);
                m_audio.playSfx(Sfx::PageChange);
            }
        }
        m_touchHitIndex = -1;
        m_touchEditDragActive = false;
    }
}

#ifdef SWITCHU_MENU
void WiiUMenuApp::handleSystemAction(SysAction a) {
    switch (a) {
        case SysAction::HomeButton:
            DebugLog::log("[pump] HomeButton -> UI update");
            m_launcher.setAppHasForeground(false);

            markSuspendedIcon(m_launcher.suspendedTitleId());
            closeActiveOverlays();
            focusTitle(m_launcher.suspendedTitleId());
            break;
        default:
            break;
    }
}
#endif

void WiiUMenuApp::updateCursor() {
    if ((m_themeShop && m_themeShop->isActive()) ||
        (m_gameGallery && m_gameGallery->isActive()) ||
        (m_gameMods && m_gameMods->isActive()) ||
        (m_gameDetails && m_gameDetails->isActive()) ||
        (m_settings && m_settings->isActive()) ||
        (m_dialog && m_dialog->isActive()) ||
        (m_userSelect && m_userSelect->isActive()))
        return;

    auto* cur = focusManager().current();
    if (cur) {
        nxui::Rect fr = cur->focusRect();
        m_cursor->moveTo(fr.expanded(4.f));
        m_cursor->setVisible(true);
    } else {
        m_cursor->setVisible(false);
    }
}

// Pressing + enters the game dossier directly. The old first dialog cost an
// unnecessary action and hid the very information the player asked for.
void WiiUMenuApp::showIconOptions() {
#ifdef SWITCHU_MENU
    if (m_editMode) return;
    if ((m_dialog && m_dialog->isActive()) || (m_gameMods && m_gameMods->isActive()) ||
        (m_gameDetails && m_gameDetails->isActive())) return;

    auto* cur = focusManager().current();
    if (!cur || cur->tag() != "glossy_icon") return;
    auto* icon = static_cast<GlossyIcon*>(cur);

    const int index = findTitleIndex(icon->titleId());
    if (index < 0) return;
    const auto& entry = m_model.at(index);


    const std::uint64_t titleId = entry.titleId;
    const std::string title = entry.title;

    m_gameDetailsReturnFocus = cur;
    showGameDetails(titleId, title, icon->texture());
#endif
}

void WiiUMenuApp::showGameOptionsMenu(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    if (!m_dialog)
        return;
    auto& i18n = nxui::I18n::instance();
    m_dialog->show(
        title,
        i18n.tr("dialog.icon_options_body", "Choose what to do with this software."),
        {
            {i18n.tr("dialog.icon_options_info", "Software information"),
             [this, titleId, title]() { showSoftwareInformation(titleId, title); }, true},
            {i18n.tr("dialog.icon_options_customize", "Customize"),
             [this, titleId, title]() { showGameCustomizeMenu(titleId, title); }, true},
            {i18n.tr("dialog.icon_options_delete", "Delete software"),
             [this, titleId, title]() { confirmDeleteSoftware(titleId, title); }, false},
            // closeOnPress, not an action. Cancel had it false with an empty
            // callback, which is a button that does nothing at all: the dialog
            // stayed open and the only way out was B.
            {i18n.tr("button.cancel", "Cancel"), [this]() {}, true},
        },
        0, {});
    focusManager().setFocus(m_dialog.get());
#endif
}

void WiiUMenuApp::showGameCustomizeMenu(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    if (!m_dialog)
        return;
    auto& i18n = nxui::I18n::instance();
    m_audio.playSfx(Sfx::ModalShow);
    m_dialog->show(
        i18n.tr("dialog.customize_title", "Customize"),
        title + "\n\n" + i18n.tr("dialog.customize_body",
                                  "Choose how this software appears on the menu."),
        {
            {i18n.tr("dialog.icon_options_gallery", "Gallery"),
             [this, titleId, title]() { showGameGallery(titleId, title); }, true},
            {i18n.tr("dialog.customize_active_art", "Active artwork"),
             [this, titleId, title]() { showGameArtworkStatus(titleId, title); }, true},
            {i18n.tr("dialog.customize_restore_default", "Restore default"),
             [this, titleId, title]() { showGameArtworkRestoreMenu(titleId, title); }, true},
        },
        0,
        [this, titleId, title]() { showGameOptionsMenu(titleId, title); });
    focusManager().setFocus(m_dialog.get());
#else
    (void)titleId;
    (void)title;
#endif
}

void WiiUMenuApp::showGameArtworkStatus(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    if (!m_dialog)
        return;
    // Game Details is added after the shared dialog in the overlay tree.  Put
    // the dialog back at the end before showing it so the modal is visible,
    // rather than merely receiving controller focus behind the dossier.
    if (m_overlayLayer) {
        m_overlayLayer->removeChild(m_dialog.get());
        m_overlayLayer->addChild(m_dialog);
    }
    auto& i18n = nxui::I18n::instance();
    const bool customCover = !gallery::GameArtworkStore::pathFor(
        titleId, gallery::ArtworkKind::Cover).empty();
    const bool customBackground = !gallery::GameArtworkStore::pathFor(
        titleId, gallery::ArtworkKind::Background).empty();
    const std::string body = title + "\n\n"
        + i18n.tr(customCover ? "dialog.customize_cover_custom"
                              : "dialog.customize_cover_default",
                  customCover ? "Cover: custom" : "Cover: default")
        + "\n"
        + i18n.tr(customBackground ? "dialog.customize_background_custom"
                                   : "dialog.customize_background_default",
                  customBackground ? "Background: custom" : "Background: default");
    // Details is now the parent view. Returning to the retired Customize
    // dialog here created two dialog transitions in one frame and could leave
    // controller focus without an active widget. This is intentionally a
    // terminal status dialog: both A and B hand focus straight back to Details.
    auto returnToDetails = [this]() {
        if (m_gameDetails && m_gameDetails->isActive())
            focusManager().setFocus(m_gameDetails.get());
    };
    m_dialog->show(i18n.tr("dialog.customize_active_art", "Active artwork"), body,
                   {{i18n.tr("button.ok", "OK"), returnToDetails, true}}, 0, returnToDetails);
    focusManager().setFocus(m_dialog.get());
#else
    (void)titleId;
    (void)title;
#endif
}

void WiiUMenuApp::showGameArtworkRestoreMenu(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    if (!m_dialog)
        return;
    if (m_overlayLayer) {
        m_overlayLayer->removeChild(m_dialog.get());
        m_overlayLayer->addChild(m_dialog);
    }
    auto& i18n = nxui::I18n::instance();
    auto returnToDetails = [this]() {
        if (m_gameDetails && m_gameDetails->isActive())
            focusManager().setFocus(m_gameDetails.get());
    };
    m_dialog->show(i18n.tr("dialog.customize_restore_default", "Restore default"),
                   title + "\n\n" + i18n.tr("dialog.customize_restore_body",
                                               "Choose the artwork to restore."),
                   {
                       {i18n.tr("dialog.customize_restore_cover", "Restore cover"),
                        [this, titleId, title]() {
                            restoreGameArtworkFromOptions(titleId, title, gallery::ArtworkKind::Cover);
                        }, true},
                       {i18n.tr("dialog.customize_restore_background", "Restore background"),
                        [this, titleId, title]() {
                            restoreGameArtworkFromOptions(titleId, title, gallery::ArtworkKind::Background);
                        }, true},
                   },
                   0,
                   returnToDetails);
    focusManager().setFocus(m_dialog.get());
#else
    (void)titleId;
    (void)title;
#endif
}

void WiiUMenuApp::restoreGameArtworkFromOptions(
    std::uint64_t titleId, const std::string& title, gallery::ArtworkKind kind) {
#ifdef SWITCHU_MENU
    if (!m_dialog)
        return;
    const auto result = gallery::GameArtworkStore::restoreDefault(titleId, kind);
    const bool cover = kind == gallery::ArtworkKind::Cover;
    if (result.ok && cover && m_grid) {
        m_iconStreamer.forceReload(m_grid->currentPage(), m_grid->iconsPerPage(),
                                   app().gpu(), app().renderer(), m_grid->allIcons());
    }
    DebugLog::log("[gallery] options restore %s: title=%016llX ok=%d error=%s",
                  cover ? "cover" : "background", static_cast<unsigned long long>(titleId),
                  result.ok ? 1 : 0, result.error.c_str());
    auto& i18n = nxui::I18n::instance();
    const char* messageKey = result.ok
        ? (cover ? "dialog.gallery_restored_cover" : "dialog.gallery_restored_background")
        : "dialog.gallery_save_failed";
    const char* fallback = result.ok
        ? (cover ? "Default cover restored." : "Default background restored.")
        : "Could not save image.";
    auto returnToDetails = [this]() {
        if (m_gameDetails && m_gameDetails->isActive())
            focusManager().setFocus(m_gameDetails.get());
    };
    m_dialog->show(i18n.tr("dialog.customize_restore_default", "Restore default"),
                   title + "\n\n" + i18n.tr(messageKey, fallback),
                   {{i18n.tr("button.ok", "OK"), returnToDetails, true}},
                   0,
                   returnToDetails);
    focusManager().setFocus(m_dialog.get());
#else
    (void)titleId;
    (void)title;
    (void)kind;
#endif
}

void WiiUMenuApp::showSoftwareInformation(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    showGameDetails(titleId, title);
#endif
}

void WiiUMenuApp::showGameDetails(std::uint64_t titleId, const std::string& title,
                                  nxui::Texture* liveCover) {
#ifdef SWITCHU_MENU
    createGameDetails();
    if (!m_gameDetails) return;
    std::vector<std::uint8_t> cover = gallery::GameArtworkStore::loadCover(titleId);
    if (cover.empty()) cover = AppListLoader::loadIconData(titleId);
    if (!m_gameDetailsReturnFocus)
        m_gameDetailsReturnFocus = m_dialogReturnFocus;
    m_dialogReturnFocus = m_gameDetails.get();
    m_audio.playSfx(Sfx::ModalShow);
    m_gameDetails->openForGame(titleId, title, std::move(cover), liveCover,
                               installedDisplayVersion(titleId), installedModSummary(titleId),
                               installedPlayTime(titleId));
    focusManager().setFocus(m_gameDetails.get());
#else
    (void)titleId;
    (void)title;
#endif
}

void WiiUMenuApp::showGameMods(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    if (!m_gameMods) {
        m_gameMods = std::make_shared<GameModsScreen>();
        if (m_overlayLayer) m_overlayLayer->addChild(m_gameMods);
        m_gameMods->setFont(&m_fontNormal);
        m_gameMods->setSmallFont(&m_fontSmall);
        m_gameMods->setTheme(&m_theme);
        m_gameMods->setAccessibilityVoiceEnabled(m_config.accessibilityEnabled);
        m_gameMods->setAccessibilitySpeechPreferences(m_config.accessibilitySpeakHints,
                                                       m_config.accessibilitySpeakPosition);
        m_gameMods->onNavigateSfx([this]() { m_audio.playSfx(Sfx::Navigate); });
        m_gameMods->onActivateSfx([this]() { m_audio.playSfx(Sfx::Activate); });
        m_gameMods->onCloseSfx([this]() { m_audio.playSfx(Sfx::ModalHide); });
        m_gameMods->onAccessibilityAnnouncement([this](const std::string& text) {
            m_accessibility.announce(text);
        });
        m_gameMods->onRequestRemove([this]() { confirmDeleteGameMod(); });
        m_gameMods->onClosed([this]() {
            if (m_gameDetails && m_gameDetails->isActive()) {
                m_suppressNextNavigateSfx = true;
                focusManager().setFocus(m_gameDetails.get());
            }
        });
    }
    m_audio.playSfx(Sfx::ModalShow);
    m_gameMods->openForGame(titleId, title);
    focusManager().setFocus(m_gameMods.get());
#else
    (void)titleId;
    (void)title;
#endif
}

void WiiUMenuApp::confirmDeleteGameMod() {
#ifdef SWITCHU_MENU
    if (!m_dialog || !m_gameMods || !m_gameMods->isActive()) return;
    const std::string name = m_gameMods->selectedName();
    if (name.empty()) return;
    auto& i18n = nxui::I18n::instance();
    // The Mods screen is newer in the overlay stack; place the confirmation
    // above it so it receives both rendering and controller focus.
    if (m_overlayLayer) {
        m_overlayLayer->removeChild(m_dialog.get());
        m_overlayLayer->addChild(m_dialog);
    }
    m_dialogReturnFocus = m_gameMods.get();
    m_dialog->show(i18n.tr("dialog.mods_remove_title", "Remove mod?"),
                   name + "\n\n" + i18n.tr("dialog.mods_remove_body",
                                              "This permanently removes this mod from the SD card."),
                   {
                       {i18n.tr("button.cancel", "Cancel"), []() {}, true},
                       {i18n.tr("dialog.mods_remove", "Remove"), [this]() {
                            if (m_gameMods) m_gameMods->removeSelected();
                        }, true},
                   }, 0, {});
    focusManager().setFocus(m_dialog.get());
#endif
}

void WiiUMenuApp::showGameGallery(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    createGameGallery();
    if (!m_gameGallery)
        return;
    // The normal caller leaves its return target in m_dialogReturnFocus. When
    // Gallery was entered from the dossier this is the dossier itself, so the
    // close callback can resume it instead of exiting to the home grid.
    m_gameGalleryReturnFocus = m_dialogReturnFocus;
    m_dialogReturnFocus = m_gameGallery.get();
    m_audio.playSfx(Sfx::ModalShow);
    m_gameGallery->openForGame(titleId, title);
    focusManager().setFocus(m_gameGallery.get());
#endif
}

void WiiUMenuApp::confirmGameArtwork(
    std::uint64_t titleId, const std::string& title, GameGalleryClient::Category category,
    GameGalleryClient::Asset asset, std::shared_ptr<const std::vector<std::uint8_t>> bytes) {
#ifdef SWITCHU_MENU
    if (!m_dialog || !m_gameGallery || !bytes || bytes->empty())
        return;

    auto& i18n = nxui::I18n::instance();
    const bool cover = category == GameGalleryClient::Category::Grids;
    const std::string message = i18n.tr(
        cover ? "dialog.gallery_apply_cover" : "dialog.gallery_apply_background",
        cover ? "This image will be saved as this software's custom cover."
              : "This image will be saved as this software's custom background.");
    // GameGallery is created after the shared dialog and therefore sits above
    // it in the overlay paint order. Reinsert the dialog before showing it so
    // its focused A/B controls and its visual card always describe one layer.
    if (m_overlayLayer) {
        m_overlayLayer->removeChild(m_dialog.get());
        m_overlayLayer->addChild(m_dialog);
    }
    m_dialogReturnFocus = m_gameGallery.get();
    m_audio.playSfx(Sfx::ModalShow);
    m_dialog->show(i18n.tr("dialog.gallery_apply_title", "Use this image?"),
                   title + "\n\n" + message,
                   {
                       {i18n.tr("button.cancel", "Cancel"), [this]() {}, true},
                       {i18n.tr("dialog.gallery_apply", "Use image"),
                        [this, titleId, category, asset = std::move(asset), bytes = std::move(bytes)]() mutable {
                            startGameArtworkSave(titleId, category, std::move(asset), std::move(bytes));
                        }, true},
                   },
                   0, {});
    focusManager().setFocus(m_dialog.get());
#else
    (void)titleId;
    (void)title;
    (void)category;
    (void)asset;
    (void)bytes;
#endif
}

void WiiUMenuApp::startGameArtworkSave(
    std::uint64_t titleId, GameGalleryClient::Category category,
    GameGalleryClient::Asset asset, std::shared_ptr<const std::vector<std::uint8_t>> bytes) {
    if (!m_gameGallery || !bytes || bytes->empty())
        return;
    syncGameArtworkSave();
    if (m_gameArtworkSaveFuture.valid()
        && m_gameArtworkSaveFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        m_gameGallery->requestToast(nxui::I18n::instance().tr(
            "dialog.gallery_saving", "Saving image..."));
        return;
    }

    const auto kind = category == GameGalleryClient::Category::Grids
        ? gallery::ArtworkKind::Cover : gallery::ArtworkKind::Background;
    auto shared = std::make_shared<GameArtworkSaveShared>();
    shared->result.kind = kind;
    m_gameArtworkSave = shared;
    m_gameGallery->requestToast(nxui::I18n::instance().tr(
        "dialog.gallery_saving", "Saving image..."));
    m_gameArtworkSaveFuture = m_threadPool.submit([shared, titleId, kind, asset = std::move(asset), bytes = std::move(bytes)]() {
        const auto result = gallery::GameArtworkStore::save(titleId, kind, asset, *bytes);
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->result = result;
    });
}

void WiiUMenuApp::confirmRestoreGameArtwork(
    std::uint64_t titleId, const std::string& title, GameGalleryClient::Category category) {
#ifdef SWITCHU_MENU
    if (!m_dialog || !m_gameGallery)
        return;

    const bool cover = category == GameGalleryClient::Category::Grids;
    auto& i18n = nxui::I18n::instance();
    if (m_overlayLayer) {
        m_overlayLayer->removeChild(m_dialog.get());
        m_overlayLayer->addChild(m_dialog);
    }
    m_dialogReturnFocus = m_gameGallery.get();
    m_audio.playSfx(Sfx::ModalShow);
    m_dialog->show(i18n.tr(cover ? "dialog.gallery_restore_cover_title"
                                 : "dialog.gallery_restore_background_title",
                                 cover ? "Restore default cover?" : "Restore default background?"),
                   title + "\n\n" + i18n.tr(
                       cover ? "dialog.gallery_restore_cover_body"
                             : "dialog.gallery_restore_background_body",
                       cover ? "The custom cover will be removed and the original Switch icon will return."
                             : "The custom background will be removed and the theme background will return."),
                   {
                       {i18n.tr("button.cancel", "Cancel"), [this]() {}, true},
                       {i18n.tr("dialog.gallery_restore", "Restore default"),
                        [this, titleId, category]() { startGameArtworkRestore(titleId, category); }, true},
                   },
                   0, {});
    focusManager().setFocus(m_dialog.get());
#else
    (void)titleId;
    (void)title;
    (void)category;
#endif
}

void WiiUMenuApp::startGameArtworkRestore(
    std::uint64_t titleId, GameGalleryClient::Category category) {
    if (!m_gameGallery)
        return;
    syncGameArtworkSave();
    if (m_gameArtworkSaveFuture.valid()
        && m_gameArtworkSaveFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        m_gameGallery->requestToast(nxui::I18n::instance().tr(
            "dialog.gallery_saving", "Saving image..."));
        return;
    }
    const auto kind = category == GameGalleryClient::Category::Grids
        ? gallery::ArtworkKind::Cover : gallery::ArtworkKind::Background;
    auto shared = std::make_shared<GameArtworkSaveShared>();
    shared->result.kind = kind;
    shared->result.titleId = titleId;
    shared->result.restored = true;
    m_gameArtworkSave = shared;
    m_gameGallery->requestToast(nxui::I18n::instance().tr(
        "dialog.gallery_restoring", "Restoring default..."));
    m_gameArtworkSaveFuture = m_threadPool.submit([shared, titleId, kind]() {
        const auto result = gallery::GameArtworkStore::restoreDefault(titleId, kind);
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->result = result;
    });
}

void WiiUMenuApp::syncGameArtworkSave() {
    if (!m_gameArtworkSaveFuture.valid()
        || m_gameArtworkSaveFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    auto shared = m_gameArtworkSave;
    gallery::ArtworkSaveResult result;
    try {
        m_gameArtworkSaveFuture.get();
    } catch (...) {
        result.error = "unexpected save failure";
    }
    if (shared) {
        std::lock_guard<std::mutex> lock(shared->mutex);
        result = shared->result;
    }
    m_gameArtworkSave.reset();
    if (!m_gameGallery)
        return;

    auto& i18n = nxui::I18n::instance();
    if (result.ok) {
        const bool cover = result.kind == gallery::ArtworkKind::Cover;
        const char* messageKey = result.restored
            ? (cover ? "dialog.gallery_restored_cover" : "dialog.gallery_restored_background")
            : (cover ? "dialog.gallery_saved_cover" : "dialog.gallery_saved_background");
        const char* fallback = result.restored
            ? (cover ? "Default cover restored." : "Default background restored.")
            : (cover ? "Custom cover saved." : "Custom background saved.");
        m_gameGallery->requestToast(i18n.tr(messageKey, fallback), 2.8f);
        DebugLog::log("[gallery] %s %s: %s", result.restored ? "restored" : "saved",
                      cover ? "cover" : "background", result.path.c_str());
        if (cover && m_grid) {
            // The visible icon may already own a streamed system texture. Reload
            // this small page cache so the new cover appears before navigating
            // away and back.
            m_iconStreamer.forceReload(m_grid->currentPage(), m_grid->iconsPerPage(),
                                       app().gpu(), app().renderer(), m_grid->allIcons());
        } else if (!cover) {
            auto* focused = focusManager().current();
            if (focused && focused->tag() == "glossy_icon" &&
                static_cast<GlossyIcon*>(focused)->titleId() == result.titleId)
                refreshGameArtworkBackdrop(result.titleId);
        }
    } else {
        m_gameGallery->requestToast(i18n.tr("dialog.gallery_save_failed",
                                            "Could not save image."), 3.2f);
        DebugLog::log("[gallery] save failed: %s", result.error.c_str());
    }
}

void WiiUMenuApp::confirmDeleteSoftware(std::uint64_t titleId, const std::string& title) {
#ifdef SWITCHU_MENU
    if (!m_dialog)
        return;
    // Reached from the dossier, which is added after the shared dialog in the
    // overlay tree. Without moving the dialog back to the end it takes focus
    // but is drawn behind the dossier: the A/B hints change and no card
    // appears, so a delete looks like it is waiting on nothing. Same fix the
    // artwork dialogs already carry.
    if (m_overlayLayer) {
        m_overlayLayer->removeChild(m_dialog.get());
        m_overlayLayer->addChild(m_dialog);
    }
    auto& i18n = nxui::I18n::instance();
    // Says what goes and what does not. Save data survives a delete on this
    // console, and somebody about to remove a game they intend to reinstall
    // deserves to know that before they decide, not after.
    std::string body = fmt::format("{}\n\n{}", title,
        i18n.tr("dialog.delete_software_body",
                "This removes the software from the console. It cannot be undone "
                "here -- the software has to be downloaded or installed again."));

    m_audio.playSfx(Sfx::ModalShow);
    m_dialog->show(
        i18n.tr("dialog.delete_software", "Delete software?"),
        body,
        {
            {i18n.tr("button.cancel", "Cancel"), [this]() {}, true},
            {i18n.tr("button.delete", "Delete"), [this, titleId]() {
                 const Result rc = nsDeleteApplicationCompletely(titleId);
                 DebugLog::log("[menu] delete 0x%016lX rc=0x%X", titleId, rc);
                 // The daemon notices the record change and asks for a
                 // refresh, which is what takes the icon off the grid. Doing
                 // it here as well would rebuild the grid from under the
                 // dialog that is still closing.
                 auto& tr = nxui::I18n::instance();
                 m_dialog->show(
                     tr.tr("dialog.delete_software", "Delete software?"),
                     R_SUCCEEDED(rc)
                         ? tr.tr("dialog.delete_software_done", "The software was deleted.")
                         : tr.tr("dialog.delete_software_failed",
                                 "The software could not be deleted."),
                     {{tr.tr("button.ok", "OK"), [this]() {}, true}},
                     0, {});
                 focusManager().setFocus(m_dialog.get());
             }, false},
        },
        0, {});
    focusManager().setFocus(m_dialog.get());
#endif
}
