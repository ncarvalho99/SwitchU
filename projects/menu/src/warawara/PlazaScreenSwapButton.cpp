#include "PlazaScreenSwapButton.hpp"
#include <nxui/core/Input.hpp>
#include <cmath>
#include <algorithm>

namespace warawara {

PlazaScreenSwapButton::PlazaScreenSwapButton() {
    setSize(54.0f, 48.0f);
    setCornerRadius(16.0f);
    setLiquidGlassEnabled(true);
    setForceLiquidGlass(true);
    setBlurEnabled(false);
    setFocusable(true);
    setTag("plaza_screen_swap");
    setAccessibilityRole("button");
    setAccessibilityLabel("Screen Swap");
    setAccessibilityHint("A to switch between Home Menu and WaraWara Plaza");
    addAction(static_cast<uint64_t>(nxui::Button::A), [this]() {
        if (m_onActivateCb) m_onActivateCb();
    });
}

void PlazaScreenSwapButton::setPlazaActive(bool active) {
    m_plazaActive = active;
    setAccessibilityLabel(active ? "Return to Home Menu" : "WaraWara Plaza");
}

void PlazaScreenSwapButton::onUpdate(float dt) {
    m_pulseAnim += dt;
}

void PlazaScreenSwapButton::onRender(nxui::Renderer& ren) {
    const float alpha = m_opacity * m_panelOpacity;
    if (alpha <= 0.01f) return;

    // Background glass pill
    const float r = cornerRadius();
    nxui::Color bgCol = m_plazaActive
        ? nxui::Color(0.12f, 0.40f, 0.85f, 0.45f * alpha)
        : nxui::Color(0.06f, 0.10f, 0.18f, 0.38f * alpha);

    ren.drawRoundedRect(m_rect, bgCol, r);

    // Outline
    const float pulse = 0.5f + 0.5f * std::sin(m_pulseAnim * 3.0f);
    nxui::Color borderCol = isFocused()
        ? nxui::Color(1.0f, 0.85f, 0.25f, (0.80f + 0.20f * pulse) * alpha)
        : (m_plazaActive
            ? nxui::Color(0.35f, 0.75f, 1.0f, 0.70f * alpha)
            : nxui::Color(1.0f, 1.0f, 1.0f, 0.32f * alpha));

    ren.drawRoundedRectOutline(m_rect, borderCol, r, isFocused() ? 2.0f : 1.2f);

    // Procedural TV / GamePad Swap Icon Glyph
    const float cx = m_rect.x + m_rect.width * 0.5f;
    const float cy = m_rect.y + m_rect.height * 0.5f;

    // TV screen icon (top-left)
    const float tvW = 22.0f;
    const float tvH = 14.0f;
    const float tvX = cx - 14.0f;
    const float tvY = cy - 11.0f;
    nxui::Rect tvRect{tvX, tvY, tvW, tvH};
    nxui::Color tvCol = (!m_plazaActive)
        ? nxui::Color(0.30f, 0.75f, 1.0f, 0.95f * alpha) // Glowing cyan highlight
        : nxui::Color(0.85f, 0.88f, 0.92f, 0.65f * alpha);
    ren.drawRoundedRectOutline(tvRect, tvCol, 3.0f, 1.5f);
    // TV base stand
    ren.drawLine({tvX + 7.0f, tvY + tvH}, {tvX + 15.0f, tvY + tvH}, tvCol, 1.5f);
    ren.drawLine({tvX + 9.0f, tvY + tvH + 2.0f}, {tvX + 13.0f, tvY + tvH + 2.0f}, tvCol, 1.5f);

    // GamePad screen icon (bottom-right)
    const float gpW = 24.0f;
    const float gpH = 13.0f;
    const float gpX = cx - 8.0f;
    const float gpY = cy - 2.0f;
    nxui::Rect gpRect{gpX, gpY, gpW, gpH};
    nxui::Color gpCol = m_plazaActive
        ? nxui::Color(0.30f, 0.75f, 1.0f, 0.95f * alpha) // Glowing cyan highlight
        : nxui::Color(0.85f, 0.88f, 0.92f, 0.65f * alpha);
    ren.drawRoundedRectOutline(gpRect, gpCol, 4.0f, 1.5f);
    // GamePad screen inner display
    nxui::Rect gpScreen{gpX + 5.0f, gpY + 2.0f, gpW - 10.0f, gpH - 4.0f};
    ren.drawRoundedRect(gpScreen, gpCol.withAlpha(0.28f * alpha), 1.5f);
    // GamePad control dots
    ren.drawCircle({gpX + 2.8f, gpY + gpH * 0.5f}, 1.2f, gpCol);
    ren.drawCircle({gpX + gpW - 2.8f, gpY + gpH * 0.5f}, 1.2f, gpCol);

    // Swap arrows indicator between TV and GamePad
    nxui::Color arrowCol(1.0f, 1.0f, 1.0f, 0.80f * alpha);
    ren.drawLine({cx - 1.0f, cy - 6.0f}, {cx + 4.0f, cy - 6.0f}, arrowCol, 1.2f);
    ren.drawLine({cx + 4.0f, cy - 6.0f}, {cx + 2.0f, cy - 8.0f}, arrowCol, 1.2f);

    ren.drawLine({cx + 1.0f, cy + 4.0f}, {cx - 4.0f, cy + 4.0f}, arrowCol, 1.2f);
    ren.drawLine({cx - 4.0f, cy + 4.0f}, {cx - 2.0f, cy + 6.0f}, arrowCol, 1.2f);
}

bool PlazaScreenSwapButton::handleTouch(const nxui::Input& input) {
    if (!isVisible() || m_opacity <= 0.05f) return false;

    if (input.touchDown()) {
        if (m_rect.contains(input.touchX(), input.touchY())) {
            if (m_onActivateCb) {
                m_onActivateCb();
            }
            return true;
        }
    }
    return false;
}

} // namespace warawara
