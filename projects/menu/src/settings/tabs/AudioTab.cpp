#include "TabBuilders.hpp"
#include <nxui/core/I18n.hpp>
#include <switch.h>
#include <algorithm>
#include <fmt/format.h>

SettingsScreen::Tab settings::tabs::AudioTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.audio", "Audio");

    // System Audio Section
    {
        SettingItem section;
        section.type = ItemType::Section;
        section.label = i18n.tr("settings.audio.section_system", "System Audio");
        t.items.push_back(std::move(section));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.audio.speaker_auto_mute", "Speaker Auto-Mute");
        it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.audio.speaker_auto_mute_desc", "Mute speakers automatically when headphones are connected.");
        bool val = false;
        setsysGetSpeakerAutoMuteFlag(&val);
        it.boolVal = val;
        it.anim01 = val ? 1.f : 0.f;
        it.onChange = [](SettingItem& self) {
            setsysSetSpeakerAutoMuteFlag(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.audio.headphone_warning", "Headphone Volume Warning");
        it.type = ItemType::Info;
        u32 count = 0;
        setsysGetHeadphoneVolumeWarningCount(&count);
        it.infoText = count > 0 ? i18n.tr("settings.audio.acknowledged", "Acknowledged")
                                : i18n.tr("common.active", "Active");
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it;
        it.label = i18n.tr("settings.audio.tv_audio_output", "TV Audio Output");
        it.type = ItemType::Selector;
        it.description = i18n.tr("settings.audio.tv_audio_output_desc", "Select audio format sent to TV over HDMI.");
        it.options = {
            i18n.tr("settings.audio.mono", "Mono"),
            i18n.tr("settings.audio.stereo", "Stereo"),
            i18n.tr("settings.audio.surround", "Surround")
        };

        SetSysAudioOutputMode mode = (SetSysAudioOutputMode)1;
        setsysGetAudioOutputMode(SetSysAudioOutputModeTarget_Unknown0, &mode);
        it.intVal = std::clamp((int)mode, 0, 2);
        it.onChange = [](SettingItem& self) {
            setsysSetAudioOutputMode(SetSysAudioOutputModeTarget_Unknown0,
                                     (SetSysAudioOutputMode)self.intVal);
        };
        t.items.push_back(std::move(it));
    }

    // Custom Soundtrack Section
    {
        SettingItem section;
        section.type = ItemType::Section;
        section.label = i18n.tr("settings.audio.section_custom_bgm", "Custom Soundtrack");
        t.items.push_back(std::move(section));
    }

    // Custom Soundtrack Toggle
    {
        SettingItem it;
        it.label = i18n.tr("settings.audio.custom_bgm_enable", "Custom Soundtrack");
        it.description = i18n.tr("settings.audio.custom_bgm_enable_desc",
                                 "Play music from sdmc:/config/SwitchU/music/ (.mp3, .ogg, .wav).");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_customBgmEnabled;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_customBgmEnabled = self.boolVal;
            if (screen.m_customBgmEnabledCb)
                screen.m_customBgmEnabledCb(self.boolVal);
            screen.rebuildCurrentTab();
        };
        t.items.push_back(std::move(it));
    }

    // Shuffle Toggle
    {
        SettingItem it;
        it.label = i18n.tr("settings.audio.custom_bgm_shuffle", "Shuffle Playback");
        it.description = i18n.tr("settings.audio.custom_bgm_shuffle_desc",
                                 "Play custom tracks in randomized order without repeats.");
        it.type = ItemType::Toggle;
        it.boolVal = screen.m_customBgmShuffle;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [&screen](SettingItem& self) {
            screen.m_customBgmShuffle = self.boolVal;
            if (screen.m_customBgmShuffleCb)
                screen.m_customBgmShuffleCb(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    // Track Count / Folder info
    {
        SettingItem it;
        it.label = i18n.tr("settings.audio.custom_bgm_folder", "Music Folder");
        it.type = ItemType::Info;
        if (screen.m_customBgmTrackCount > 0) {
            it.infoText = fmt::format(fmt::runtime(i18n.tr("settings.audio.custom_bgm_tracks_found", "{} tracks detected")),
                                      screen.m_customBgmTrackCount);
        } else {
            it.infoText = i18n.tr("settings.audio.custom_bgm_no_tracks", "0 tracks (sdmc:/config/SwitchU/music/)");
        }
        t.items.push_back(std::move(it));
    }

    // Rescan Folder Action
    {
        SettingItem it;
        it.label = i18n.tr("settings.audio.custom_bgm_rescan", "Rescan Music Folder");
        it.description = i18n.tr("settings.audio.custom_bgm_rescan_desc",
                                 "Scan SD card for newly added audio tracks.");
        it.buttonLabel = i18n.tr("button.rescan", "Rescan");
        it.type = ItemType::Action;
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_customBgmRescanCb)
                screen.m_customBgmRescanCb();
            screen.rebuildCurrentTab();
        };
        t.items.push_back(std::move(it));
    }

    // Track list / preview items
    if (!screen.m_customBgmTrackTitles.empty()) {
        SettingItem listHeader;
        listHeader.type = ItemType::Section;
        listHeader.label = i18n.tr("settings.audio.custom_bgm_tracklist", "Track List & Preview");
        t.items.push_back(std::move(listHeader));

        for (int i = 0; i < static_cast<int>(screen.m_customBgmTrackTitles.size()); ++i) {
            SettingItem trackItem;
            trackItem.type = ItemType::Action;
            trackItem.label = fmt::format("{}. {}", i + 1, screen.m_customBgmTrackTitles[i]);
            const bool isCurrent = (screen.m_customBgmCurrentTrack == i && screen.m_customBgmEnabled);
            trackItem.buttonLabel = isCurrent
                ? i18n.tr("settings.audio.custom_bgm_playing", "Playing")
                : i18n.tr("settings.audio.custom_bgm_play", "Play");
            trackItem.description = isCurrent
                ? i18n.tr("settings.audio.custom_bgm_now_playing", "Currently playing on home menu")
                : i18n.tr("settings.audio.custom_bgm_preview_hint", "Select to play this track now");
            const int trackIndex = i;
            trackItem.onChange = [&screen, trackIndex](SettingItem&) {
                if (screen.m_customBgmPreviewTrackCb)
                    screen.m_customBgmPreviewTrackCb(trackIndex);
                screen.rebuildCurrentTab();
            };
            t.items.push_back(std::move(trackItem));
        }
    }

    return t;
}
