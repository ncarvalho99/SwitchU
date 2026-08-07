#include "ThemeShopTabBuilders.hpp"

#include <nxui/core/I18n.hpp>
#include <algorithm>
#include <cmath>

ThemeShopScreen::Tab themeshop::tabs::OptionsTab::build(ThemeShopScreen& screen) {
    using Tab = ThemeShopScreen::Tab;
    using SettingItem = ThemeShopScreen::SettingItem;
    using ItemType = ThemeShopScreen::ItemType;
    auto& i18n = nxui::I18n::instance();

    Tab t;
    t.name = i18n.tr("themeshop.tabs.options", "Options");

    {
        SettingItem it;
        it.label = i18n.tr("settings.music.menu_music", "Menu Music");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_musicEnabled;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_musicEnabled = self.boolVal;
            if (screen.m_musicEnabledCb) screen.m_musicEnabledCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.music.music_volume", "Music Volume");
        it.type = ItemType::Slider;
        it.floatVal = std::clamp(screen.m_musicVolume, 0.f, 1.f);
        it.anim01 = it.floatVal;
        it.onChange = [&screen](SettingItem& self) {
            self.anim01 = self.floatVal;
            screen.m_musicVolume = std::clamp(self.floatVal, 0.f, 1.f);
            if (screen.m_musicVolumeCb) screen.m_musicVolumeCb(screen.m_musicVolume);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.music.sfx_volume", "SFX Volume");
        it.type = ItemType::Slider;
        it.floatVal = std::clamp(screen.m_sfxVolume, 0.f, 1.f);
        it.anim01 = it.floatVal;
        it.onChange = [&screen](SettingItem& self) {
            self.anim01 = self.floatVal;
            screen.m_sfxVolume = std::clamp(self.floatVal, 0.f, 1.f);
            if (screen.m_sfxVolumeCb) screen.m_sfxVolumeCb(screen.m_sfxVolume);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.display.glass_sharpness", "Glass sharpness");
        it.description = i18n.tr("settings.display.glass_sharpness_desc",
                                 "How clearly the screen behind menus shows through them.");
        it.type = ItemType::Slider;
        it.floatVal = std::clamp(screen.m_glassSharpness, 0.f, 1.f);
        it.anim01 = it.floatVal;
        it.onChange = [&screen](SettingItem& self) {
            self.anim01 = self.floatVal;
            screen.m_glassSharpness = std::clamp(self.floatVal, 0.f, 1.f);
            if (screen.m_glassSharpnessCb) screen.m_glassSharpnessCb(screen.m_glassSharpness);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.display.background_speed", "Background animation speed");
        it.description = i18n.tr("settings.display.background_speed_desc",
                                 "How fast the shapes drift. Centre is the theme's own pace.");
        it.type = ItemType::Slider;
        it.floatVal = std::clamp(screen.m_backgroundSpeed, 0.f, 1.f);
        it.anim01 = it.floatVal;
        it.onChange = [&screen](SettingItem& self) {
            self.anim01 = self.floatVal;
            screen.m_backgroundSpeed = std::clamp(self.floatVal, 0.f, 1.f);
            if (screen.m_backgroundSpeedCb) screen.m_backgroundSpeedCb(screen.m_backgroundSpeed);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.display.background_blur", "Background blur");
        it.description = i18n.tr("settings.display.background_blur_desc",
                                 "Soften the wallpaper and the shapes drifting over it.");
        it.type = ItemType::Slider;
        it.floatVal = std::clamp(screen.m_backgroundBlur, 0.f, 1.f);
        it.anim01 = it.floatVal;
        it.onChange = [&screen](SettingItem& self) {
            self.anim01 = self.floatVal;
            screen.m_backgroundBlur = std::clamp(self.floatVal, 0.f, 1.f);
            if (screen.m_backgroundBlurCb) screen.m_backgroundBlurCb(screen.m_backgroundBlur);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.display.grid_columns", "Home Grid Columns");
        it.type = ItemType::Slider;
        it.sliderSteps = 5;
        int cols = std::clamp(screen.m_gridColumns, 3, 8);
        it.floatVal = (float)(cols - 3) / 5.f;
        it.anim01 = it.floatVal;
        it.infoText = std::to_string(cols);
        it.onChange = [&screen](SettingItem& self) {
            int cols = std::clamp(3 + (int)std::round(std::clamp(self.floatVal, 0.f, 1.f) * 5.f), 3, 8);
            self.floatVal = (float)(cols - 3) / 5.f;
            self.anim01 = self.floatVal;
            self.infoText = std::to_string(cols);
            screen.m_gridColumns = cols;
            if (screen.m_gridColumnsCb) screen.m_gridColumnsCb(cols);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.display.grid_rows", "Home Grid Rows");
        it.type = ItemType::Slider;
        it.sliderSteps = 3;
        int rows = std::clamp(screen.m_gridRows, 2, 5);
        it.floatVal = (float)(rows - 2) / 3.f;
        it.anim01 = it.floatVal;
        it.infoText = std::to_string(rows);
        it.onChange = [&screen](SettingItem& self) {
            int rows = std::clamp(2 + (int)std::round(std::clamp(self.floatVal, 0.f, 1.f) * 3.f), 2, 5);
            self.floatVal = (float)(rows - 2) / 3.f;
            self.anim01 = self.floatVal;
            self.infoText = std::to_string(rows);
            screen.m_gridRows = rows;
            if (screen.m_gridRowsCb) screen.m_gridRowsCb(rows);
        };
        t.items.push_back(std::move(it));
    }

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
        it.label = i18n.tr("themeshop.options.restart_menu", "Restart the menu");
        // Listing homebrew only takes effect on a restart, and "restart the
        // menu" is not something a person is expected to know how to do --
        // the console offers no such command. Asking them to do it without
        // giving them a way was the gap.
        it.description = i18n.tr("themeshop.options.restart_menu_desc",
                                 "Needed for newly listed homebrew to appear.");
        it.type = ItemType::Action;
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_restartMenuCb) screen.m_restartMenuCb();
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("themeshop.options.homebrew_host", "Applet homebrew replaces");
        // The console gives no warning of its own, so the description is the
        // only place someone finds out what they are agreeing to.
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

    return t;
}
