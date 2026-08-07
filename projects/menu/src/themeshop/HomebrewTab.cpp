#include "ThemeShopTabBuilders.hpp"
#include "../launcher/HomebrewScanner.hpp"
#include <nxui/core/I18n.hpp>
#include <algorithm>

// A row per file plus a row per action does not survive a card with a hundred
// .nro on it -- the first version of this pushed the last settings tab off the
// window on a card with forty-two. One picker names the file and the two
// actions below it act on whatever it names, so the tab is three rows whether
// the card holds four homebrew or four hundred.
ThemeShopScreen::Tab themeshop::tabs::HomebrewTab::build(ThemeShopScreen& screen) {
    using Tab = ThemeShopScreen::Tab;
    using SettingItem = ThemeShopScreen::SettingItem;
    using ItemType = ThemeShopScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("themeshop.tabs.homebrew", "Homebrew");

    {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.show", "Show homebrew");
        it.description = i18n.tr("settings.homebrew.show_desc",
                                 "List .nro files from the card. Applies when the menu restarts.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_showHomebrew;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_showHomebrew = self.boolVal;
            if (screen.m_showHomebrewCb) screen.m_showHomebrewCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("themeshop.options.restart_menu", "Restart the menu");
        // Everything on this tab -- listing, hiding -- only lands on a restart,
        // and the only way to do one was a button on another tab. Repeating it
        // here is not clutter; sending someone two tabs away to finish what they
        // started here is.
        it.description = i18n.tr("themeshop.options.restart_menu_desc",
                                 "Needed for newly listed homebrew to appear.");
        it.type = ItemType::Action;
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_restartMenuCb) screen.m_restartMenuCb();
        };
        t.items.push_back(std::move(it));
    }

    // Walked here rather than taken from the menu's list, which is what is left
    // after hiding -- a hidden file would have nothing to come back from.
    auto entries = homebrew::scan("sdmc:/switch");
    std::sort(entries.begin(), entries.end(),
              [](const homebrew::Entry& a, const homebrew::Entry& b) {
                  return a.name < b.name;
              });

    if (entries.empty()) {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.none", "No .nro files found on the card.");
        it.type = ItemType::Info;
        t.items.push_back(std::move(it));
        return t;
    }

    screen.m_homebrewPaths.clear();
    std::vector<std::string> names;
    names.reserve(entries.size());
    for (const auto& hb : entries) {
        screen.m_homebrewPaths.push_back(hb.path);
        names.push_back(hb.name);
    }
    screen.m_homebrewIndex =
        std::clamp(screen.m_homebrewIndex, 0, (int)names.size() - 1);

    {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.pick", "Homebrew");
        it.description = screen.selectedHomebrewPath();
        it.type = ItemType::Selector;
        it.options = names;
        it.intVal = screen.m_homebrewIndex;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_homebrewIndex = self.intVal;
            // The path is the identity, and it is the one thing about a
            // selection that is not obvious from the name.
            self.description = screen.selectedHomebrewPath();
            if (screen.m_homebrewSelectionCb) screen.m_homebrewSelectionCb();
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.in_menu", "Show in the menu");
        it.description = i18n.tr("settings.homebrew.hide_desc",
                                 "Hidden entries stay on the card, just not in the grid.");
        it.type = ItemType::Toggle;
        it.boolVal = !screen.isHomebrewHidden(screen.selectedHomebrewPath());
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            if (screen.m_homebrewHiddenCb)
                screen.m_homebrewHiddenCb(screen.selectedHomebrewPath(), !self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.delete", "Delete this file");
        it.description = i18n.tr("settings.homebrew.delete_desc",
                                 "Removes the .nro only. Its folder and settings stay.");
        it.type = ItemType::Action;
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_homebrewDeleteCb)
                screen.m_homebrewDeleteCb(screen.selectedHomebrewPath());
        };
        t.items.push_back(std::move(it));
    }

    return t;
}
