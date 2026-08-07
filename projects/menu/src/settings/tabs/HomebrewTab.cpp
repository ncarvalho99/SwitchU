#include "TabBuilders.hpp"
#include "../../launcher/HomebrewScanner.hpp"
#include <nxui/core/I18n.hpp>
#include <algorithm>

// Everything about homebrew in one place. The listing switch used to sit under
// Display, beside brightness and burn-in, which is not where anyone looked for
// it -- the same reason the appearance sliders moved out of there.
//
// The per-file actions are here rather than in Storage on purpose. Storage
// uninstalls titles, and a title comes back from the eShop; an .nro deleted
// from the card does not come back at all. Giving those two the same shape in
// the same list invites a press that costs more than it looks like it will.
SettingsScreen::Tab settings::tabs::HomebrewTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.homebrew", "Homebrew");

    {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.show", "Show homebrew");
        it.description = i18n.tr("settings.homebrew.show_desc",
                                 "List .nro files from the card. Applies when the menu restarts.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.showHomebrew();
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            if (screen.m_showHomebrewCb)
                screen.m_showHomebrewCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    // The card is walked here rather than reusing the menu's list, because this
    // has to show what is hidden too -- the menu's list is what is left after
    // hiding, so a hidden entry would have no row to un-hide it from.
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

    {
        SettingItem it;
        it.label = i18n.tr("settings.homebrew.files", "Files on the card");
        it.type = ItemType::Section;
        t.items.push_back(std::move(it));
    }

    for (const auto& hb : entries) {
        const std::string path = hb.path;

        {
            SettingItem it;
            it.label = hb.name;
            it.wrapLabel = true;
            it.description = i18n.tr("settings.homebrew.hide_desc",
                                     "Hidden entries stay on the card, just not in the grid.");
            it.type = ItemType::Toggle;
            it.boolVal = !screen.isHomebrewHidden(path);
            it.anim01 = it.boolVal ? 1.f : 0.f;
            it.onChange = [&screen, path](SettingItem& self) {
                // The switch reads as "in the menu", so it is on when visible.
                if (screen.m_homebrewHiddenCb)
                    screen.m_homebrewHiddenCb(path, !self.boolVal);
            };
            t.items.push_back(std::move(it));
        }

        {
            SettingItem it;
            it.label = i18n.tr("settings.homebrew.delete", "Delete this file");
            it.description = path;
            it.type = ItemType::Action;
            it.onChange = [&screen, path](SettingItem&) {
                // Only asks. What it costs is spelled out in the dialog the
                // menu raises, and nothing is removed until that is answered.
                if (screen.m_homebrewDeleteCb)
                    screen.m_homebrewDeleteCb(path);
            };
            t.items.push_back(std::move(it));
        }
    }

    return t;
}
