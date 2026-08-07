#include "ThemeShopTabBuilders.hpp"
#include "../launcher/HomebrewScanner.hpp"
#include <nxui/core/I18n.hpp>
#include <algorithm>

// Everything about homebrew, in the order someone meets it: turn it on, choose
// what it costs, restart to see it, then manage the files.
//
// The tab is a fixed number of rows whatever the card holds. A row per file
// plus a row per action pushed the About tab off the settings window on a card
// with forty-two .nro; one picker names the file and the rows below it act on
// what it names.
ThemeShopScreen::Tab themeshop::tabs::HomebrewTab::build(ThemeShopScreen& screen) {
    using Tab = ThemeShopScreen::Tab;
    using SettingItem = ThemeShopScreen::SettingItem;
    using ItemType = ThemeShopScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("themeshop.tabs.homebrew", "Homebrew");

    {
        SettingItem it;
        it.label = i18n.tr("themeshop.options.homebrew_launch", "Launch homebrew");
        it.description = i18n.tr("themeshop.options.homebrew_launch_desc",
                                 "Opens .nro files from the grid. Takes over one system applet.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_homebrewLaunch;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_homebrewLaunch = self.boolVal;
            if (screen.m_homebrewLaunchCb) screen.m_homebrewLaunchCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("themeshop.options.homebrew_host", "Applet homebrew replaces");
        // The console warns about none of this, so the description does.
        it.description = i18n.tr("themeshop.options.homebrew_host_desc",
                                 "Parental controls stop being enforced, or the Album stops "
                                 "showing screenshots. Hold R while launching for the real one.");
        it.type = ItemType::Selector;
        it.options = {
            i18n.tr("themeshop.options.host_parental", "Parental controls"),
            i18n.tr("themeshop.options.host_album", "Album"),
        };
        it.intVal = std::clamp(screen.m_homebrewHost, 0, 1);
        it.onChange = [&screen](SettingItem& self) {
            screen.m_homebrewHost = std::clamp(self.intVal, 0, 1);
            if (screen.m_homebrewHostCb) screen.m_homebrewHostCb(screen.m_homebrewHost);
        };
        t.items.push_back(std::move(it));
    }

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
        // Listing and hiding both only land on a restart, and sending someone
        // to another tab to finish what they started here is the real clutter.
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

    const std::size_t pickerIdx = t.items.size();
    {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.pick", "Homebrew");
        it.description = screen.selectedHomebrewPath();
        it.type = ItemType::Selector;
        it.options = names;
        it.intVal = screen.m_homebrewIndex;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_homebrewIndex = self.intVal;
        };
        t.items.push_back(std::move(it));
    }

    const std::size_t toggleIdx = t.items.size();
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

    // The rows below the picker describe whatever it points at, so they have to
    // follow it. Reading the state once at build time left them showing the
    // first selection for ever: picking a second homebrew kept the first one's
    // switch, and toggling it would have acted on the wrong file. Rebuilding
    // the tab from inside an item's own callback would free that item mid-call,
    // so the sync happens here, once a frame, where nothing is being destroyed.
    t.onUpdate = [&screen, pickerIdx, toggleIdx](Tab& tab, TabbedOverlayScreen&) {
        if (toggleIdx >= tab.items.size())
            return;
        const std::string path = screen.selectedHomebrewPath();
        if (pickerIdx < tab.items.size())
            tab.items[pickerIdx].description = path;

        auto& toggle = tab.items[toggleIdx];
        const bool visible = !screen.isHomebrewHidden(path);
        if (toggle.boolVal != visible) {
            toggle.boolVal = visible;
            toggle.anim01 = visible ? 1.f : 0.f;
        }
    };

    return t;
}
