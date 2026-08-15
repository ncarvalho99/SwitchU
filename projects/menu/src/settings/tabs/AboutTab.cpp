#include "TabBuilders.hpp"
#include <nxui/core/I18n.hpp>

#ifndef SWITCHU_VERSION
#define SWITCHU_VERSION "unknown"
#endif

#ifndef SWITCHU_UPSTREAM_VERSION
#define SWITCHU_UPSTREAM_VERSION "1.1.0"
#endif

// This build is a fork, and the two extra rows exist for reasons beyond
// vanity: the source link pointed at the upstream repository, so a bug
// introduced here would have been reported to someone who never wrote it, and
// GPL-2.0 section 2(a) asks a modified version to say that it was modified.
// PoloNX stays named above the fork, which is both correct and the point.

SettingsScreen::Tab settings::tabs::AboutTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.about", "About");

    auto info = [&](const char* key, const char* fallback, std::string value) {
        SettingItem it;
        it.label    = i18n.tr(key, fallback);
        it.type     = ItemType::Info;
        it.infoText = std::move(value);
        t.items.push_back(std::move(it));
    };

    info("settings.about.version", "Version", "SwitchU " SWITCHU_VERSION);

    // Sits under the version it is about. The launcher also checks once a day
    // on its own; this is for somebody who does not want to wait for that.
    {
        SettingItem it;
        it.label = i18n.tr("settings.about.check_updates", "Check for updates");
        it.description = i18n.tr("settings.about.check_updates_desc",
                                 "Asks GitHub whether a newer version has been released.");
        it.type = ItemType::Action;
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_checkUpdatesCb) screen.m_checkUpdatesCb();
        };
        t.items.push_back(std::move(it));
    }
    info("settings.about.based_on", "Based on", "SwitchU " SWITCHU_UPSTREAM_VERSION);
    info("settings.about.author", "Original author", "PoloNX");
    info("settings.about.fork_maintainer", "Fork maintained by", "ncarvalho99");
    info("settings.about.license", "License",
         i18n.tr("settings.about.license_value", "GPL-2.0"));
    info("settings.about.source_code", "Source code (fork)",
         i18n.tr("settings.about.source_code_value", "github.com/ncarvalho99/SwitchU"));
    info("settings.about.upstream_source", "Source code (upstream)",
         i18n.tr("settings.about.upstream_source_value", "github.com/PoloNX/SwitchU"));

    // Acknowledgements
    {
        SettingItem it;
        it.label = i18n.tr("settings.about.acknowledgements", "Acknowledgements");
        it.type  = ItemType::Section;
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.about.acknowledgements_desc",
                           "Thanks to xortroll for your help with your project uLaunch "
                           "and for your support and advice during development.");
        it.type  = ItemType::Info;
        it.infoText = "";
        // Long enough to run off the panel in all eight translations, and it
        // did: only the value column was ever fitted to width.
        it.wrapLabel = true;
        t.items.push_back(std::move(it));
    }

    return t;
}
