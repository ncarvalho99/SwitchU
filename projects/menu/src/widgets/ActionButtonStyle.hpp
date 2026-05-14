#pragma once

#include <nxui/widgets/GlassBox.hpp>
#include <nxui/Theme.hpp>
#include <algorithm>

namespace switchu::ui {

struct ActionButtonVisualStyle {
    nxui::Color baseColor;
    nxui::Color borderColor;
    nxui::Color highlightColor;
    float borderWidth = 1.2f;
    float scale = 1.f;
};

inline ActionButtonVisualStyle resolveActionButtonStyle(const nxui::Theme* theme,
                                                        float opacity,
                                                        float emphasis,
                                                        float accentMix = -1.f) {
    ActionButtonVisualStyle style;

    emphasis = std::clamp(emphasis, 0.f, 1.f);
    accentMix = accentMix < 0.f ? emphasis : std::clamp(accentMix, 0.f, 1.f);

    float pulse = 0.18f + 0.16f * emphasis;

    nxui::Color idleBase = theme ? theme->panelBase.withAlpha(0.10f * opacity)
                                 : nxui::Color(0.28f, 0.32f, 0.38f, 0.10f * opacity);
    nxui::Color accentBase = theme ? theme->cursorNormal.withAlpha(pulse * opacity)
                                   : nxui::Color(0.20f, 0.46f, 0.86f, pulse * opacity);
    nxui::Color idleBorder = theme ? theme->panelBorder.withAlpha(0.40f * opacity)
                                   : nxui::Color(0.5f, 0.5f, 0.65f, 0.40f * opacity);
    nxui::Color accentBorder = theme ? theme->panelBorder.withAlpha(0.65f * opacity)
                                     : nxui::Color(0.5f, 0.5f, 0.65f, 0.65f * opacity);
    nxui::Color idleHighlight = theme ? theme->panelHighlight.withAlpha(0.03f * opacity)
                                      : nxui::Color(1.f, 1.f, 1.f, 0.03f * opacity);
    nxui::Color accentHighlight = theme ? theme->panelHighlight.withAlpha(0.09f * opacity)
                                        : nxui::Color(1.f, 1.f, 1.f, 0.09f * opacity);

    style.baseColor = nxui::Color::lerp(idleBase, accentBase, accentMix);
    style.borderColor = nxui::Color::lerp(idleBorder, accentBorder, accentMix);
    style.highlightColor = nxui::Color::lerp(idleHighlight, accentHighlight, accentMix);
    style.borderWidth = 1.2f + 0.35f * accentMix;
    style.scale = 1.f + 0.03f * accentMix;
    return style;
}

inline void prepareActionButton(nxui::GlassBox& button, float cornerRadius) {
    button.setPadding(9.f, 14.f, 9.f, 14.f);
    button.setAlignItems(nxui::AlignItems::CENTER);
    button.setJustifyContent(nxui::JustifyContent::CENTER);
    button.setCornerRadius(cornerRadius);
    button.setBorderWidth(1.2f);
}

inline void applyActionButtonStyle(nxui::GlassBox& button,
                                   const nxui::Theme* theme,
                                   float opacity,
                                   float emphasis,
                                   float accentMix = -1.f) {
    ActionButtonVisualStyle style = resolveActionButtonStyle(theme, opacity, emphasis, accentMix);
    button.setBaseColor(style.baseColor);
    button.setBorderColor(style.borderColor);
    button.setHighlightColor(style.highlightColor);
    button.setBorderWidth(style.borderWidth);
    button.setScale(style.scale);
}

} // namespace switchu::ui