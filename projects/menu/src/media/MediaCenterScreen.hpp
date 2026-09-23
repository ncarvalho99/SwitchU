#pragma once

#include <nxui/widgets/GlassWidget.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/Theme.hpp>
#include <nxui/core/Input.hpp>
#include "core/AudioManager.hpp"
#include <vector>
#include <string>
#include <functional>

namespace media {

class MediaCenterScreen final : public nxui::GlassWidget {
public:
    MediaCenterScreen();
    ~MediaCenterScreen() override = default;

    void setFont(nxui::Font* font) { m_font = font; }
    void setSmallFont(nxui::Font* font) { m_smallFont = font; }
    void setTheme(const nxui::Theme* theme) { m_theme = theme; }

    void show();
    void hide();
    bool isActive() const { return m_active; }

    void setPlaybackState(bool playing, bool paused, bool shuffle,
                          int currentTrackIndex, const std::string& currentTitle,
                          const std::vector<TrackInfo>& tracks,
                          float volume, const std::string& audioMode);

    void onPlayPause(std::function<void()> cb) { m_onPlayPauseCb = std::move(cb); }
    void onNextTrack(std::function<void()> cb) { m_onNextTrackCb = std::move(cb); }
    void onPrevTrack(std::function<void()> cb) { m_onPrevTrackCb = std::move(cb); }
    void onShuffleToggle(std::function<void()> cb) { m_onShuffleToggleCb = std::move(cb); }
    void onPlayTrack(std::function<void(int)> cb) { m_onPlayTrackCb = std::move(cb); }
    void onAudioModeChange(std::function<void(const std::string&)> cb) { m_onAudioModeChangeCb = std::move(cb); }
    void onVolumeChange(std::function<void(float)> cb) { m_onVolumeChangeCb = std::move(cb); }
    void onRescan(std::function<void()> cb) { m_onRescanCb = std::move(cb); }
    void onClose(std::function<void()> cb) { m_onCloseCb = std::move(cb); }

    void handleInput(const nxui::Input& input, float dt);
    void handleTouch(const nxui::Input& input);

protected:
    void onUpdate(float dt) override;
    void onRender(nxui::Renderer& ren) override;

private:
    bool m_active = false;
    bool m_backdropCacheValid = false;
    float m_fadeAnim = 0.0f;
    float m_animTimer = 0.0f;

    nxui::Font* m_font = nullptr;
    nxui::Font* m_smallFont = nullptr;
    const nxui::Theme* m_theme = nullptr;

    bool m_playing = false;
    bool m_paused = false;
    bool m_shuffle = false;
    int  m_currentTrack = -1;
    std::string m_currentTitle;
    std::vector<TrackInfo> m_tracks;
    float m_volume = 0.15f;
    std::string m_audioMode = "custom_first";

    // Focus state
    // Row 0: Transport buttons (0: Prev, 1: Play/Pause, 2: Next, 3: Shuffle)
    // Row 1: Audio Settings (0: Audio Mode, 1: Vol-, 2: Vol+, 3: Rescan)
    // Row 2: Playlist tracks (selected track index)
    int m_focusRow = 0;
    int m_focusCol = 1;
    int m_focusedTrack = 0;
    int m_listScrollOffset = 0;

    // Marquee scroll
    float m_marqueeOffset = 0.0f;
    float m_marqueeWait = 0.0f;
    int   m_marqueeDir = 1;

    // Callbacks
    std::function<void()> m_onPlayPauseCb;
    std::function<void()> m_onNextTrackCb;
    std::function<void()> m_onPrevTrackCb;
    std::function<void()> m_onShuffleToggleCb;
    std::function<void(int)> m_onPlayTrackCb;
    std::function<void(const std::string&)> m_onAudioModeChangeCb;
    std::function<void(float)> m_onVolumeChangeCb;
    std::function<void()> m_onRescanCb;
    std::function<void()> m_onCloseCb;

    void cycleAudioMode(int dir);
    void adjustVolume(float delta);
    std::string getAudioModeLabel() const;
    std::string getAudioModeDesc() const;
};

} // namespace media
