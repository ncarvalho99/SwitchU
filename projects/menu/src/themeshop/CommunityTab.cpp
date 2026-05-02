#include "ThemeShopTabBuilders.hpp"

#include <nxui/core/I18n.hpp>

#include <algorithm>

ThemeShopScreen::Tab themeshop::tabs::CommunityTab::build(ThemeShopScreen& screen) {
    using Tab = ThemeShopScreen::Tab;
    auto& i18n = nxui::I18n::instance();

    Tab t;
    t.name = i18n.tr("themeshop.tabs.community", "Community");
    t.onUpdate = [&screen](Tab&, TabbedOverlayScreen&) {
        if (screen.m_threadPool
            && screen.m_communityRevision == 0
            && screen.m_communityTransferState.phase() == ThemeTransferState::Phase::Idle) {
            screen.refreshCommunityCatalog();
        }
        if (screen.pollCommunityCatalog())
            screen.rebuildCurrentTab();
    };

    return t;
}