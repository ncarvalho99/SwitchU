#include "ActionButton.hpp"

#include <algorithm>

ActionButton::ActionButton() {
    setAxis(nxui::Axis::ROW);
    setPadding(9.f, 14.f, 9.f, 14.f);
    setAlignItems(nxui::AlignItems::CENTER);
    setJustifyContent(nxui::JustifyContent::CENTER);
    setCornerRadius(12.f);
    setLiquidGlassEnabled(false);
    setForceLiquidGlass(false);
    setLiquidGlassShaderEnabled(false);
    setBlurEnabled(false);
    setBackingEnabled(false);
    setMaterialTextureEnabled(false);
    setBorderWidth(0.f);
}

void ActionButton::setVisualState(float opacity, float emphasis, float accentMix) {
    m_styleOpacity = std::clamp(opacity, 0.f, 1.f);
    m_emphasis = std::clamp(emphasis, 0.f, 1.f);
    m_accentMix = accentMix;
}

void ActionButton::onRender(nxui::Renderer& ren) {
    auto style = switchu::ui::resolveActionButtonStyle(
        m_theme,
        m_opacity * m_panelOpacity * m_styleOpacity,
        m_emphasis,
        m_accentMix);
    switchu::ui::drawActionButtonChrome(ren, m_rect, cornerRadius(), style);
}
