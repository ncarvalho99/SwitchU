#include "ThemeShopTabBuilders.hpp"

#include <nxui/core/I18n.hpp>

// The animated catalogue, kept apart from the community one because the two
// hold different things and are maintained by different people. PoloNX's index
// carries themes built for his release; this one carries the moving wallpapers,
// which are large, hosted separately, and ours to add to.
ThemeShopScreen::Tab themeshop::tabs::AnimatedTab::build(ThemeShopScreen& screen) {
    using Tab = ThemeShopScreen::Tab;
    auto& i18n = nxui::I18n::instance();

    Tab t;
    t.name = i18n.tr("themeshop.tabs.animated", "Animated Themes");
    t.onUpdate = [&screen](Tab&, TabbedOverlayScreen&) {
        // Both catalogues arrive in one fetch and are told apart by where each
        // entry came from, so this tab does not start a second download.
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
