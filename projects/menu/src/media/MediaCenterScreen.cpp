#include "MediaCenterScreen.hpp"
#include "settings/SettingsGlassTuning.hpp"
#include <nxui/core/I18n.hpp>
#include <fmt/format.h>
#include <algorithm>
#include <cmath>

namespace media {

namespace {

std::string truncateText(nxui::Font* font, const std::string& text, float scale, float maxW) {
    if (!font || text.empty() || maxW <= 4.f) return text;
    if (font->measure(text).x * scale <= maxW) return text;

    constexpr const char* kEllipsis = "...";
    std::string out = text;
    while (!out.empty()) {
        out.pop_back();
        std::string cand = out + kEllipsis;
        if (font->measure(cand).x * scale <= maxW) return cand;
    }
    return kEllipsis;
}

} // namespace

MediaCenterScreen::MediaCenterScreen() {
    setRect({0.f, 0.f, 1280.f, 720.f});
    setLiquidGlassEnabled(true);
    setForceLiquidGlass(true);
    setBlurEnabled(false);
    setFocusable(true);
    setTag("media_center_screen");
    setAccessibilityRole("window");
    setAccessibilityLabel("Multimedia Center");
}

void MediaCenterScreen::show() {
    m_active = true;
    m_backdropCacheValid = false;
    m_fadeAnim = 0.0f;
    m_focusRow = 0;
    m_focusCol = 1; // Play/Pause
    m_marqueeOffset = 0.0f;
    m_marqueeWait = 1.5f;
    m_marqueeDir = 1;
}

void MediaCenterScreen::hide() {
    m_active = false;
    if (m_onCloseCb) m_onCloseCb();
}

void MediaCenterScreen::setPlaybackState(bool playing, bool paused, bool shuffle,
                                        int currentTrackIndex, const std::string& currentTitle,
                                        const std::vector<TrackInfo>& tracks,
                                        float volume, const std::string& audioMode) {
    m_playing = playing;
    m_paused = paused;
    m_shuffle = shuffle;
    m_currentTrack = currentTrackIndex;
    if (m_currentTitle != currentTitle) {
        m_currentTitle = currentTitle;
        m_marqueeOffset = 0.0f;
        m_marqueeWait = 1.5f;
        m_marqueeDir = 1;
    }
    m_tracks = tracks;
    m_volume = volume;
    m_audioMode = audioMode;
    if (m_focusedTrack >= static_cast<int>(m_tracks.size())) {
        m_focusedTrack = std::max(0, static_cast<int>(m_tracks.size()) - 1);
    }
}

void MediaCenterScreen::onUpdate(float dt) {
    if (!m_active) return;
    m_animTimer += dt;
    if (m_fadeAnim < 1.0f) {
        m_fadeAnim = std::min(1.0f, m_fadeAnim + dt * 6.0f);
    }

    // Ping-pong marquee update for long titles
    if (m_font && !m_currentTitle.empty()) {
        const float maxW = 790.0f;
        const float textW = m_font->measure(m_currentTitle).x * 0.95f;
        if (textW > maxW) {
            const float maxOffset = textW - maxW;
            if (m_marqueeWait > 0.0f) {
                m_marqueeWait -= dt;
            } else {
                m_marqueeOffset += m_marqueeDir * dt * 45.0f;
                if (m_marqueeOffset >= maxOffset) {
                    m_marqueeOffset = maxOffset;
                    m_marqueeDir = -1;
                    m_marqueeWait = 2.0f;
                } else if (m_marqueeOffset <= 0.0f) {
                    m_marqueeOffset = 0.0f;
                    m_marqueeDir = 1;
                    m_marqueeWait = 2.0f;
                }
            }
        } else {
            m_marqueeOffset = 0.0f;
        }
    }
}

std::string MediaCenterScreen::getAudioModeLabel() const {
    auto& i18n = nxui::I18n::instance();
    if (m_audioMode == "custom_only")
        return i18n.tr("media.mode.custom_only", "Custom SD Only");
    if (m_audioMode == "theme_first")
        return i18n.tr("media.mode.theme_first", "Theme Music First");
    if (m_audioMode == "theme_only")
        return i18n.tr("media.mode.theme_only", "Theme & Preset Only");
    return i18n.tr("media.mode.custom_first", "Custom SD First");
}

std::string MediaCenterScreen::getAudioModeDesc() const {
    auto& i18n = nxui::I18n::instance();
    if (m_audioMode == "custom_only")
        return i18n.tr("media.mode.custom_only_desc", "Plays only SD card music. Theme music is ignored.");
    if (m_audioMode == "theme_first")
        return i18n.tr("media.mode.theme_first_desc", "Theme music takes precedence. Falls back to SD card if none.");
    if (m_audioMode == "theme_only")
        return i18n.tr("media.mode.theme_only_desc", "Plays theme or preset sounds only. SD music ignored.");
    return i18n.tr("media.mode.custom_first_desc", "Plays SD card music. Falls back to theme music if folder empty.");
}

void MediaCenterScreen::cycleAudioMode(int dir) {
    static const std::vector<std::string> kModes = {
        "custom_first", "theme_first", "custom_only", "theme_only"
    };
    auto it = std::find(kModes.begin(), kModes.end(), m_audioMode);
    int idx = (it != kModes.end()) ? static_cast<int>(std::distance(kModes.begin(), it)) : 0;
    idx = (idx + dir + static_cast<int>(kModes.size())) % static_cast<int>(kModes.size());
    m_audioMode = kModes[idx];
    if (m_onAudioModeChangeCb) m_onAudioModeChangeCb(m_audioMode);
}

void MediaCenterScreen::adjustVolume(float delta) {
    m_volume = std::clamp(m_volume + delta, 0.0f, 1.0f);
    if (m_onVolumeChangeCb) m_onVolumeChangeCb(m_volume);
}

void MediaCenterScreen::onRender(nxui::Renderer& ren) {
    if (!m_active || m_fadeAnim <= 0.01f) return;

    auto& i18n = nxui::I18n::instance();
    const float alpha = m_fadeAnim;
    const float pulse = 0.5f + 0.5f * std::sin(m_animTimer * 3.0f);
    const nxui::Color focusGold(1.0f, 0.85f, 0.25f, (0.85f + 0.15f * pulse) * alpha);
    const nxui::Color accentCyan(0.20f, 0.70f, 0.95f, alpha);

    // 1. Soft Backdrop Scrim
    ren.drawRect({0.f, 0.f, 1280.f, 720.f}, nxui::Color(0.0f, 0.0f, 0.0f, 0.62f * alpha));

    // 2. Liquid Glass Modal Window
    const nxui::Rect winRect{190.0f, 75.0f, 900.0f, 570.0f};
    const auto& tuning = settings::debug::settingsGlassTuning();

    if (!m_backdropCacheValid) {
        ren.captureToOffscreenSharp();
        if (tuning.blurIterations > 0 && tuning.preBlurRadius > 0.001f) {
            ren.applyBlur(tuning.preBlurRadius, tuning.blurIterations);
        }
        ren.copyOffscreen(nxui::GpuDevice::OFF_SHARP_A, 1);
        m_backdropCacheValid = true;
    }

    nxui::LiquidGlassSettings savedGlass = ren.liquidGlassSettings();
    auto& glass = ren.liquidGlassSettings();
    glass.refractionIntensity = std::clamp(tuning.refractionIntensity, 0.0f, 1.5f);
    glass.blurIntensity = std::max(0.0f, tuning.shaderBlurIntensity);
    glass.noiseIntensity = 0.0f;
    glass.glowIntensity = std::max(0.0f, tuning.glowIntensity);
    glass.saturation = std::max(0.0f, tuning.saturation);
    glass.opacityMultiplier = 1.0f;
    glass.roughness = std::max(0.0f, tuning.roughness);
    glass.powerFactor = std::max(1.001f, tuning.powerFactor);

    nxui::Color glassTint = m_theme ? m_theme->panelBase.withAlpha(m_theme->mode == nxui::ThemeMode::Dark
        ? std::clamp(tuning.tintAlphaDark, 0.0f, 1.0f)
        : std::clamp(tuning.tintAlphaLight, 0.0f, 1.0f))
        : nxui::Color(0.08f, 0.14f, 0.24f, 0.65f);

    ren.drawLiquidGlass(1, winRect, 24.0f, glassTint, 1.0f, std::clamp(tuning.shade, 0.0f, 1.0f));
    ren.drawRoundedRectOutline(winRect,
                               m_theme ? m_theme->panelBorder.withAlpha(0.28f * alpha) : nxui::Color(1.0f, 1.0f, 1.0f, 0.28f * alpha),
                               24.0f, 1.4f);
    ren.drawRoundedRectOutline(winRect.shrunk(1.5f),
                               m_theme ? m_theme->panelHighlight.withAlpha(0.08f * alpha) : nxui::Color(1.0f, 1.0f, 1.0f, 0.08f * alpha),
                               22.5f, 1.0f);
    ren.liquidGlassSettings() = savedGlass;

    // 3. Header: Window Title & Close Hint
    if (m_font) {
        ren.drawText(i18n.tr("media.title", "Central Multimídia"), {215.0f, 92.0f}, m_font,
                     nxui::Color(1.0f, 1.0f, 1.0f, alpha), 1.05f);
    }
    if (m_smallFont) {
        const nxui::Rect closePill{940.0f, 90.0f, 125.0f, 32.0f};
        ren.drawRoundedRect(closePill, nxui::Color(0.08f, 0.14f, 0.24f, 0.45f * alpha), 12.0f);
        ren.drawRoundedRectOutline(closePill, nxui::Color(1.0f, 1.0f, 1.0f, 0.25f * alpha), 12.0f, 1.0f);
        ren.drawText(i18n.tr("button.close_hint", "(B) Fechar"), {closePill.x + 14.0f, closePill.y + 6.0f},
                     m_smallFont, nxui::Color(0.85f, 0.90f, 0.95f, alpha), 0.82f);
    }

    // 4. Now Playing Card
    const nxui::Rect bannerRect{215.0f, 135.0f, 850.0f, 75.0f};
    ren.drawRoundedRect(bannerRect, nxui::Color(0.04f, 0.08f, 0.15f, 0.60f * alpha), 16.0f);
    ren.drawRoundedRectOutline(bannerRect, nxui::Color(0.30f, 0.60f, 0.90f, 0.35f * alpha), 16.0f, 1.0f);

    // Now Playing Icon
    ren.drawCircle({bannerRect.x + 28.0f, bannerRect.y + 37.0f}, 12.0f,
                   m_playing ? nxui::Color(0.15f, 0.70f, 0.95f, 0.85f * alpha) : nxui::Color(0.5f, 0.5f, 0.5f, 0.5f * alpha));

    // Now Playing Title & Status Subtitle
    if (m_font) {
        std::string displayTitle = m_currentTitle.empty()
            ? i18n.tr("media.no_track", "Nenhuma música em reprodução")
            : m_currentTitle;

        // Push scissor or clip to avoid overflowing banner
        const float clipX = bannerRect.x + 55.0f;
        const float clipW = 770.0f;
        std::string fittedTitle = truncateText(m_font, displayTitle, 0.94f, clipW);
        if (m_marqueeOffset > 0.0f) {
            // Draw shifted by marquee
            ren.drawText(displayTitle, {clipX - m_marqueeOffset, bannerRect.y + 12.0f},
                         m_font, nxui::Color(1.0f, 1.0f, 1.0f, alpha), 0.94f);
        } else {
            ren.drawText(fittedTitle, {clipX, bannerRect.y + 12.0f},
                         m_font, nxui::Color(1.0f, 1.0f, 1.0f, alpha), 0.94f);
        }
    }

    if (m_smallFont) {
        std::string statusStr;
        if (m_playing) {
            statusStr = fmt::format(fmt::runtime(i18n.tr("media.status_playing", "Tocando • Faixa {} de {}")),
                                    m_currentTrack + 1, std::max<size_t>(1, m_tracks.size()));
        } else if (m_paused) {
            statusStr = i18n.tr("media.status_paused", "Pausado");
        } else {
            statusStr = i18n.tr("media.status_stopped", "Parado");
        }
        ren.drawText(statusStr, {bannerRect.x + 55.0f, bannerRect.y + 44.0f},
                     m_smallFont, nxui::Color(0.40f, 0.85f, 1.0f, 0.90f * alpha), 0.78f);
    }

    // 5. Row 0: Transport Buttons Bar
    const float transY = 222.0f;
    const float transH = 44.0f;
    const float transW = 200.0f;
    const float transGap = 16.0f;

    struct ButtonDef {
        std::string label;
        bool active = false;
    };
    std::vector<ButtonDef> transButtons = {
        { i18n.tr("media.btn_prev", "|<< Anterior"), false },
        { m_playing ? i18n.tr("media.btn_pause", "|| Pausar") : i18n.tr("media.btn_play", "> Tocar"), m_playing },
        { i18n.tr("media.btn_next", "Próxima >>|"), false },
        { m_shuffle ? i18n.tr("media.btn_shuffle_on", "Aleatório: SIM") : i18n.tr("media.btn_shuffle_off", "Aleatório: NÃO"), m_shuffle }
    };

    for (int i = 0; i < 4; ++i) {
        const nxui::Rect btnRect{215.0f + i * (transW + transGap), transY, transW, transH};
        const bool isFocused = (m_focusRow == 0 && m_focusCol == i);

        nxui::Color btnBg = isFocused
            ? nxui::Color(0.16f, 0.40f, 0.70f, 0.50f * alpha)
            : (transButtons[i].active
                ? nxui::Color(0.10f, 0.35f, 0.55f, 0.42f * alpha)
                : nxui::Color(0.06f, 0.12f, 0.22f, 0.36f * alpha));

        ren.drawRoundedRect(btnRect, btnBg, 14.0f);
        ren.drawRoundedRectOutline(btnRect, isFocused ? focusGold : nxui::Color(1.0f, 1.0f, 1.0f, 0.28f * alpha),
                                   14.0f, isFocused ? 2.0f : 1.0f);

        if (m_smallFont) {
            float textW = m_smallFont->measure(transButtons[i].label).x * 0.85f;
            float textX = btnRect.x + (btnRect.width - textW) * 0.5f;
            ren.drawText(transButtons[i].label, {textX, btnRect.y + 11.0f},
                         m_smallFont, isFocused ? nxui::Color(1.0f, 0.95f, 0.65f, alpha) : nxui::Color::white().withAlpha(alpha),
                         0.85f);
        }
    }

    // 6. Row 1: Audio Settings & Source Preference Bar
    const float setY = 278.0f;
    // Source Mode Pill (Col 0)
    const nxui::Rect modeRect{215.0f, setY, 410.0f, 48.0f};
    const bool modeFocused = (m_focusRow == 1 && m_focusCol == 0);
    ren.drawRoundedRect(modeRect, modeFocused ? nxui::Color(0.16f, 0.40f, 0.70f, 0.50f * alpha) : nxui::Color(0.06f, 0.12f, 0.22f, 0.36f * alpha), 14.0f);
    ren.drawRoundedRectOutline(modeRect, modeFocused ? focusGold : nxui::Color(1.0f, 1.0f, 1.0f, 0.28f * alpha), 14.0f, modeFocused ? 2.0f : 1.0f);

    if (m_smallFont) {
        std::string modeTitle = fmt::format("< {} >", getAudioModeLabel());
        ren.drawText(modeTitle, {modeRect.x + 14.0f, modeRect.y + 6.0f},
                     m_smallFont, modeFocused ? nxui::Color(1.0f, 0.95f, 0.65f, alpha) : nxui::Color::white().withAlpha(alpha),
                     0.84f);
        ren.drawText(truncateText(m_smallFont, getAudioModeDesc(), 0.70f, 380.0f), {modeRect.x + 14.0f, modeRect.y + 26.0f},
                     m_smallFont, nxui::Color(0.65f, 0.80f, 0.95f, 0.85f * alpha), 0.70f);
    }

    // Volume Down / Up Buttons (Col 1 & 2)
    const nxui::Rect volDownRect{640.0f, setY + 4.0f, 42.0f, 40.0f};
    const bool volDownFocused = (m_focusRow == 1 && m_focusCol == 1);
    ren.drawRoundedRect(volDownRect, volDownFocused ? nxui::Color(0.20f, 0.45f, 0.75f, 0.55f * alpha) : nxui::Color(0.08f, 0.14f, 0.24f, 0.40f * alpha), 12.0f);
    ren.drawRoundedRectOutline(volDownRect, volDownFocused ? focusGold : nxui::Color(1.0f, 1.0f, 1.0f, 0.25f * alpha), 12.0f, volDownFocused ? 2.0f : 1.0f);
    if (m_font) ren.drawText("-", {volDownRect.x + 14.0f, volDownRect.y + 6.0f}, m_font, nxui::Color::white().withAlpha(alpha), 0.9f);

    // Volume text
    if (m_smallFont) {
        std::string volStr = fmt::format("{:d}%", static_cast<int>(m_volume * 100.0f));
        ren.drawText(volStr, {694.0f, setY + 16.0f}, m_smallFont, nxui::Color::white().withAlpha(alpha), 0.84f);
    }

    const nxui::Rect volUpRect{742.0f, setY + 4.0f, 42.0f, 40.0f};
    const bool volUpFocused = (m_focusRow == 1 && m_focusCol == 2);
    ren.drawRoundedRect(volUpRect, volUpFocused ? nxui::Color(0.20f, 0.45f, 0.75f, 0.55f * alpha) : nxui::Color(0.08f, 0.14f, 0.24f, 0.40f * alpha), 12.0f);
    ren.drawRoundedRectOutline(volUpRect, volUpFocused ? focusGold : nxui::Color(1.0f, 1.0f, 1.0f, 0.25f * alpha), 12.0f, volUpFocused ? 2.0f : 1.0f);
    if (m_font) ren.drawText("+", {volUpRect.x + 12.0f, volUpRect.y + 6.0f}, m_font, nxui::Color::white().withAlpha(alpha), 0.9f);

    // Rescan Folder Button (Col 3)
    const nxui::Rect rescanRect{800.0f, setY + 4.0f, 265.0f, 40.0f};
    const bool rescanFocused = (m_focusRow == 1 && m_focusCol == 3);
    ren.drawRoundedRect(rescanRect, rescanFocused ? nxui::Color(0.20f, 0.45f, 0.75f, 0.55f * alpha) : nxui::Color(0.08f, 0.14f, 0.24f, 0.40f * alpha), 12.0f);
    ren.drawRoundedRectOutline(rescanRect, rescanFocused ? focusGold : nxui::Color(1.0f, 1.0f, 1.0f, 0.25f * alpha), 12.0f, rescanFocused ? 2.0f : 1.0f);
    if (m_smallFont) {
        std::string rescanStr = i18n.tr("media.rescan", "Reescanear Pasta");
        float rW = m_smallFont->measure(rescanStr).x * 0.82f;
        ren.drawText(rescanStr, {rescanRect.x + (rescanRect.width - rW) * 0.5f, rescanRect.y + 10.0f},
                     m_smallFont, rescanFocused ? nxui::Color(1.0f, 0.95f, 0.65f, alpha) : nxui::Color::white().withAlpha(alpha),
                     0.82f);
    }

    // 7. Row 2: Playlist Section Header
    const float listHeaderY = 338.0f;
    if (m_smallFont) {
        std::string tracklistHeader = fmt::format(fmt::runtime(i18n.tr("media.tracklist_count", "Lista de Faixas ({} faixas)")), m_tracks.size());
        ren.drawText(tracklistHeader, {218.0f, listHeaderY}, m_smallFont, nxui::Color(0.70f, 0.85f, 1.0f, alpha), 0.85f);
    }

    // 8. Playlist Scroll View
    const float listY = 362.0f;
    const float rowH = 46.0f;
    const float rowGap = 6.0f;
    constexpr int kVisibleRows = 5;

    if (m_tracks.empty()) {
        const nxui::Rect emptyRect{215.0f, listY, 850.0f, 150.0f};
        ren.drawRoundedRect(emptyRect, nxui::Color(0.04f, 0.08f, 0.15f, 0.35f * alpha), 14.0f);
        if (m_smallFont) {
            std::string emptyStr = i18n.tr("media.empty_folder", "Nenhuma faixa encontrada em sdmc:/config/SwitchU/music/\nAdicione arquivos .mp3, .ogg ou .wav no cartão SD.");
            ren.drawText(emptyStr, {245.0f, listY + 50.0f}, m_smallFont, nxui::Color(0.70f, 0.75f, 0.85f, 0.85f * alpha), 0.85f);
        }
    } else {
        // Adjust scroll offset to keep focused track in view
        if (m_focusRow == 2) {
            if (m_focusedTrack < m_listScrollOffset) {
                m_listScrollOffset = m_focusedTrack;
            } else if (m_focusedTrack >= m_listScrollOffset + kVisibleRows) {
                m_listScrollOffset = m_focusedTrack - kVisibleRows + 1;
            }
        }
        m_listScrollOffset = std::clamp(m_listScrollOffset, 0, std::max(0, static_cast<int>(m_tracks.size()) - kVisibleRows));

        for (int vi = 0; vi < kVisibleRows; ++vi) {
            int trackIdx = m_listScrollOffset + vi;
            if (trackIdx >= static_cast<int>(m_tracks.size())) break;

            const nxui::Rect rowRect{215.0f, listY + vi * (rowH + rowGap), 850.0f, rowH};
            const bool isFocused = (m_focusRow == 2 && m_focusedTrack == trackIdx);
            const bool isCurrentlyPlaying = (m_currentTrack == trackIdx && m_playing);

            nxui::Color rowBg = isFocused
                ? nxui::Color(0.18f, 0.42f, 0.72f, 0.50f * alpha)
                : (isCurrentlyPlaying
                    ? nxui::Color(0.08f, 0.30f, 0.50f, 0.38f * alpha)
                    : nxui::Color(0.05f, 0.10f, 0.18f, 0.30f * alpha));

            ren.drawRoundedRect(rowRect, rowBg, 12.0f);
            ren.drawRoundedRectOutline(rowRect, isFocused ? focusGold : nxui::Color(1.0f, 1.0f, 1.0f, 0.22f * alpha),
                                       12.0f, isFocused ? 2.0f : 0.8f);

            // Left track label (Guaranteed width: max 690px so it NEVER overlaps right pill!)
            if (m_smallFont) {
                std::string trackText = fmt::format("{:d}. {}", trackIdx + 1, m_tracks[trackIdx].title);
                std::string safeText = truncateText(m_smallFont, trackText, 0.86f, 680.0f);

                nxui::Color textCol = isCurrentlyPlaying
                    ? nxui::Color(0.35f, 0.85f, 1.0f, alpha)
                    : (isFocused ? nxui::Color(1.0f, 0.95f, 0.65f, alpha) : nxui::Color::white().withAlpha(alpha));

                ren.drawText(safeText, {rowRect.x + 16.0f, rowRect.y + 13.0f}, m_smallFont, textCol, 0.86f);
            }

            // Right Status Pill (Guaranteed reserved column 120px)
            const nxui::Rect pillRect{rowRect.x + rowRect.width - 128.0f, rowRect.y + 7.0f, 114.0f, 32.0f};
            nxui::Color pillBg = isCurrentlyPlaying
                ? nxui::Color(0.15f, 0.65f, 0.90f, 0.60f * alpha)
                : nxui::Color(0.10f, 0.18f, 0.28f, 0.45f * alpha);

            ren.drawRoundedRect(pillRect, pillBg, 10.0f);
            ren.drawRoundedRectOutline(pillRect,
                                       isCurrentlyPlaying ? nxui::Color(0.40f, 0.85f, 1.0f, 0.80f * alpha) : nxui::Color(1.0f, 1.0f, 1.0f, 0.20f * alpha),
                                       10.0f, 1.0f);

            if (m_smallFont) {
                std::string pillText = isCurrentlyPlaying
                    ? i18n.tr("media.playing", "Tocando")
                    : i18n.tr("media.play", "Tocar");
                float pW = m_smallFont->measure(pillText).x * 0.78f;
                ren.drawText(pillText, {pillRect.x + (pillRect.width - pW) * 0.5f, pillRect.y + 7.0f},
                             m_smallFont, isCurrentlyPlaying ? nxui::Color::white().withAlpha(alpha) : nxui::Color(0.80f, 0.85f, 0.95f, 0.90f * alpha),
                             0.78f);
            }
        }
    }
}

void MediaCenterScreen::handleInput(const nxui::Input& input, float dt) {
    (void)dt;
    if (!m_active) return;

    if (input.isDown(nxui::Button::B)) {
        hide();
        return;
    }

    // Quick Play/Pause shortcut with X
    if (input.isDown(nxui::Button::X)) {
        if (m_onPlayPauseCb) m_onPlayPauseCb();
        return;
    }

    // Quick Rescan shortcut with Y
    if (input.isDown(nxui::Button::Y)) {
        if (m_onRescanCb) m_onRescanCb();
        return;
    }

    // Vertical navigation
    if (input.isDown(nxui::Button::DUp) || input.isDown(nxui::Button::LStickU)) {
        if (m_focusRow == 2) {
            if (m_focusedTrack > 0) {
                m_focusedTrack--;
            } else {
                m_focusRow = 1;
                m_focusCol = 0;
            }
        } else if (m_focusRow == 1) {
            m_focusRow = 0;
            m_focusCol = std::clamp(m_focusCol, 0, 3);
        }
    } else if (input.isDown(nxui::Button::DDown) || input.isDown(nxui::Button::LStickD)) {
        if (m_focusRow == 0) {
            m_focusRow = 1;
            m_focusCol = 0;
        } else if (m_focusRow == 1) {
            if (!m_tracks.empty()) {
                m_focusRow = 2;
            }
        } else if (m_focusRow == 2) {
            if (m_focusedTrack + 1 < static_cast<int>(m_tracks.size())) {
                m_focusedTrack++;
            }
        }
    }

    // Horizontal navigation
    if (input.isDown(nxui::Button::DLeft) || input.isDown(nxui::Button::LStickL)) {
        if (m_focusRow == 0) {
            m_focusCol = (m_focusCol - 1 + 4) % 4;
        } else if (m_focusRow == 1) {
            if (m_focusCol == 0) {
                cycleAudioMode(-1);
            } else {
                m_focusCol = std::max(0, m_focusCol - 1);
            }
        } else if (m_focusRow == 2) {
            // Page up
            m_focusedTrack = std::max(0, m_focusedTrack - 5);
        }
    } else if (input.isDown(nxui::Button::DRight) || input.isDown(nxui::Button::LStickR)) {
        if (m_focusRow == 0) {
            m_focusCol = (m_focusCol + 1) % 4;
        } else if (m_focusRow == 1) {
            if (m_focusCol == 0) {
                cycleAudioMode(1);
            } else {
                m_focusCol = std::min(3, m_focusCol + 1);
            }
        } else if (m_focusRow == 2) {
            // Page down
            m_focusedTrack = std::min(std::max(0, static_cast<int>(m_tracks.size()) - 1), m_focusedTrack + 5);
        }
    }

    // Action A
    if (input.isDown(nxui::Button::A)) {
        if (m_focusRow == 0) {
            switch (m_focusCol) {
                case 0: if (m_onPrevTrackCb) m_onPrevTrackCb(); break;
                case 1: if (m_onPlayPauseCb) m_onPlayPauseCb(); break;
                case 2: if (m_onNextTrackCb) m_onNextTrackCb(); break;
                case 3: if (m_onShuffleToggleCb) m_onShuffleToggleCb(); break;
            }
        } else if (m_focusRow == 1) {
            switch (m_focusCol) {
                case 0: cycleAudioMode(1); break;
                case 1: adjustVolume(-0.05f); break;
                case 2: adjustVolume(+0.05f); break;
                case 3: if (m_onRescanCb) m_onRescanCb(); break;
            }
        } else if (m_focusRow == 2) {
            if (m_focusedTrack >= 0 && m_focusedTrack < static_cast<int>(m_tracks.size())) {
                if (m_onPlayTrackCb) m_onPlayTrackCb(m_focusedTrack);
            }
        }
    }
}

void MediaCenterScreen::handleTouch(const nxui::Input& input) {
    if (!m_active || !input.touchDown()) return;
    const float tx = input.touchX();
    const float ty = input.touchY();

    // Close button
    const nxui::Rect closePill{940.0f, 90.0f, 125.0f, 32.0f};
    if (closePill.contains(tx, ty)) {
        hide();
        return;
    }

    // Transport buttons (Row 0)
    const float transY = 222.0f;
    const float transH = 44.0f;
    const float transW = 200.0f;
    const float transGap = 16.0f;
    for (int i = 0; i < 4; ++i) {
        const nxui::Rect btnRect{215.0f + i * (transW + transGap), transY, transW, transH};
        if (btnRect.contains(tx, ty)) {
            m_focusRow = 0;
            m_focusCol = i;
            switch (i) {
                case 0: if (m_onPrevTrackCb) m_onPrevTrackCb(); break;
                case 1: if (m_onPlayPauseCb) m_onPlayPauseCb(); break;
                case 2: if (m_onNextTrackCb) m_onNextTrackCb(); break;
                case 3: if (m_onShuffleToggleCb) m_onShuffleToggleCb(); break;
            }
            return;
        }
    }

    // Row 1: Mode & Vol & Rescan
    const float setY = 278.0f;
    const nxui::Rect modeRect{215.0f, setY, 410.0f, 48.0f};
    if (modeRect.contains(tx, ty)) {
        m_focusRow = 1;
        m_focusCol = 0;
        cycleAudioMode(1);
        return;
    }

    const nxui::Rect volDownRect{640.0f, setY + 4.0f, 42.0f, 40.0f};
    if (volDownRect.contains(tx, ty)) {
        m_focusRow = 1;
        m_focusCol = 1;
        adjustVolume(-0.05f);
        return;
    }

    const nxui::Rect volUpRect{742.0f, setY + 4.0f, 42.0f, 40.0f};
    if (volUpRect.contains(tx, ty)) {
        m_focusRow = 1;
        m_focusCol = 2;
        adjustVolume(+0.05f);
        return;
    }

    const nxui::Rect rescanRect{800.0f, setY + 4.0f, 265.0f, 40.0f};
    if (rescanRect.contains(tx, ty)) {
        m_focusRow = 1;
        m_focusCol = 3;
        if (m_onRescanCb) m_onRescanCb();
        return;
    }

    // Row 2: Playlist
    const float listY = 362.0f;
    const float rowH = 46.0f;
    const float rowGap = 6.0f;
    constexpr int kVisibleRows = 5;
    for (int vi = 0; vi < kVisibleRows; ++vi) {
        int trackIdx = m_listScrollOffset + vi;
        if (trackIdx >= static_cast<int>(m_tracks.size())) break;

        const nxui::Rect rowRect{215.0f, listY + vi * (rowH + rowGap), 850.0f, rowH};
        if (rowRect.contains(tx, ty)) {
            m_focusRow = 2;
            m_focusedTrack = trackIdx;
            if (m_onPlayTrackCb) m_onPlayTrackCb(trackIdx);
            return;
        }
    }
}

} // namespace media
