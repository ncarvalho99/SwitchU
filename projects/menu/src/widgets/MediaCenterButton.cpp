#include "MediaCenterButton.hpp"
#include <nxui/core/Input.hpp>
#include <cmath>
#include <algorithm>

namespace widgets {

MediaCenterButton::MediaCenterButton() {
    setSize(54.0f, 48.0f);
    setCornerRadius(16.0f);
    setLiquidGlassEnabled(true);
    setForceLiquidGlass(true);
    setBlurEnabled(false);
    setFocusable(true);
    setTag("media_center_button");
    setAccessibilityRole("button");
    setAccessibilityLabel("Media Center");
    setAccessibilityHint("A to open Multimedia Center");
    addAction(static_cast<uint64_t>(nxui::Button::A), [this]() {
        if (m_onActivateCb) m_onActivateCb();
    });
}

void MediaCenterButton::setPlaying(bool playing) {
    m_playing = playing;
    setAccessibilityLabel(playing ? "Media Center (Playing)" : "Media Center (Paused)");
}

void MediaCenterButton::onUpdate(float dt) {
    m_animTime += dt;
}

void MediaCenterButton::onRender(nxui::Renderer& ren) {
    const float alpha = m_opacity * m_panelOpacity;
    if (alpha <= 0.01f) return;

    const float r = cornerRadius();
    const float pulse = 0.5f + 0.5f * std::sin(m_animTime * 3.0f);

    // Background glass pill
    nxui::Color bgCol = m_playing
        ? nxui::Color(0.08f, 0.35f, 0.55f, (0.35f + 0.10f * pulse) * alpha)
        : nxui::Color(0.06f, 0.10f, 0.18f, 0.38f * alpha);

    ren.drawRoundedRect(m_rect, bgCol, r);

    // Outline
    nxui::Color borderCol = isFocused()
        ? nxui::Color(1.0f, 0.85f, 0.25f, (0.80f + 0.20f * pulse) * alpha)
        : (m_playing
            ? nxui::Color(0.30f, 0.75f, 0.95f, (0.55f + 0.20f * pulse) * alpha)
            : nxui::Color(1.0f, 1.0f, 1.0f, 0.32f * alpha));

    ren.drawRoundedRectOutline(m_rect, borderCol, r, isFocused() ? 2.0f : 1.2f);

    // Music Note Icon (♫)
    const float cx = m_rect.x + m_rect.width * 0.5f;
    const float cy = m_rect.y + m_rect.height * 0.5f;

    nxui::Color iconCol = m_playing
        ? nxui::Color(0.35f, 0.85f, 1.0f, alpha)
        : (isFocused() ? nxui::Color(1.0f, 0.95f, 0.60f, alpha) : nxui::Color(1.0f, 1.0f, 1.0f, 0.90f * alpha));

    // Left notehead
    ren.drawCircle({cx - 7.0f, cy + 4.0f}, 4.2f, iconCol);
    // Right notehead
    ren.drawCircle({cx + 6.5f, cy + 1.5f}, 4.2f, iconCol);

    // Left stem
    ren.drawRect({cx - 4.2f, cy - 9.0f, 2.4f, 13.0f}, iconCol);
    // Right stem
    ren.drawRect({cx + 9.3f, cy - 11.5f, 2.4f, 13.0f}, iconCol);

    // Connecting beam
    ren.drawRect({cx - 4.2f, cy - 11.5f, 15.9f, 3.8f}, iconCol);
}

bool MediaCenterButton::handleTouch(const nxui::Input& input) {
    if (!isVisible() || m_opacity <= 0.05f) return false;
    if (input.touchDown()) {
        if (m_rect.contains(input.touchX(), input.touchY())) {
            if (m_onActivateCb) {
                m_onActivateCb();
                return true;
            }
        }
    }
    return false;
}

} // namespace widgets
