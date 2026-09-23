#include "ThemeShopTabBuilders.hpp"

#include <nxui/core/I18n.hpp>

ThemeShopScreen::Tab themeshop::tabs::MusicTab::build(ThemeShopScreen& screen) {
    using Tab = ThemeShopScreen::Tab;
    auto& i18n = nxui::I18n::instance();

    Tab t;
    t.name = i18n.tr("themeshop.tabs.music", "Music");
    t.onUpdate = [&screen](Tab&, TabbedOverlayScreen&) {
        screen.pollMusicDownloads();
    };

    return t;
}
