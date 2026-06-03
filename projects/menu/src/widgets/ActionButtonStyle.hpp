#pragma once

#include <nxui/core/Renderer.hpp>
#include <nxui/Theme.hpp>
#include <algorithm>
#include <cmath>

namespace switchu::ui {

struct ActionButtonVisualStyle {
    nxui::Color baseColor;
    nxui::Color borderColor;
    nxui::Color highlightColor;
    nxui::Color outerGlowColor;
    float borderWidth = 2.6f;
    float scale = 1.f;
    float materialIntensity = 0.6f;
};

inline nxui::Rect scaledActionButtonRect(const nxui::Rect& rect, float scale) {
    if (std::abs(scale - 1.f) <= 0.001f) {
        return rect;
    }

    float width = rect.width * scale;
    float height = rect.height * scale;
    return {
        rect.x + (rect.width - width) * 0.5f,
        rect.y + (rect.height - height) * 0.5f,
        width,
        height
    };
}

inline ActionButtonVisualStyle resolveActionButtonStyle(const nxui::Theme* theme,
                                                        float opacity,
                                                        float emphasis,
                                                        float accentMix = -1.f) {
    ActionButtonVisualStyle style;

    emphasis = std::clamp(emphasis, 0.f, 1.f);
    accentMix = accentMix < 0.f ? emphasis : std::clamp(accentMix, 0.f, 1.f);

    bool light = theme && theme->mode == nxui::ThemeMode::Light;
    float pulse = 0.18f + 0.10f * emphasis;

    nxui::Color panelBase = theme ? theme->panelBase
                                  : nxui::Color(0.24f, 0.28f, 0.34f, 0.42f);
    nxui::Color accent = theme ? theme->cursorNormal
                               : nxui::Color(0.20f, 0.46f, 0.86f, 1.f);
    nxui::Color shine = theme ? theme->panelHighlight
                              : nxui::Color::white();

    nxui::Color ceramic = light
        ? nxui::Color(0.80f, 0.84f, 0.88f, 0.68f)
        : nxui::Color(0.17f, 0.19f, 0.22f, 0.60f);
    nxui::Color idleBase = nxui::Color::lerp(ceramic, panelBase, light ? 0.38f : 0.42f)
                               .withAlpha((light ? 0.66f : 0.58f) * opacity);
    nxui::Color accentBase = nxui::Color::lerp(idleBase, accent, 0.38f)
                                 .withAlpha((idleBase.a + pulse * 0.35f) * opacity);
    nxui::Color idleBorder = nxui::Color::lerp(shine, panelBase.lighter(light ? 1.05f : 1.45f), light ? 0.30f : 0.48f)
                                 .withAlpha((light ? 0.64f : 0.52f) * opacity);
    nxui::Color accentBorder = accent.lighter(1.22f).withAlpha((light ? 0.72f : 0.78f) * opacity);
    nxui::Color idleHighlight = shine.withAlpha((light ? 0.25f : 0.13f) * opacity);
    nxui::Color accentHighlight = nxui::Color::lerp(shine, accent.lighter(1.35f), 0.45f)
                                      .withAlpha((light ? 0.30f : 0.18f) * opacity);

    style.baseColor = nxui::Color::lerp(idleBase, accentBase, accentMix);
    style.borderColor = nxui::Color::lerp(idleBorder, accentBorder, accentMix);
    style.highlightColor = nxui::Color::lerp(idleHighlight, accentHighlight, accentMix);
    style.outerGlowColor = nxui::Color::black().withAlpha((0.055f + 0.025f * emphasis) * opacity);
    style.borderWidth = 2.35f + 0.65f * accentMix;
    style.scale = 1.f + 0.018f * accentMix;
    style.materialIntensity = (0.62f + 0.18f * emphasis) * opacity;
    return style;
}

inline void drawActionButtonMaterial(nxui::Renderer& ren,
                                     const nxui::Rect& rect,
                                     float radius,
                                     const ActionButtonVisualStyle& style) {
    float mat = std::clamp(style.materialIntensity, 0.f, 1.f);
    if (mat <= 0.01f)
        return;

    nxui::Rect inner = rect.shrunk(1.5f);
    ren.drawRoundedRect(inner,
                        nxui::Color::white().withAlpha(0.022f * mat),
                        std::max(0.f, radius - 1.5f));

    nxui::Rect upper = {
        inner.x + 2.f,
        inner.y + 2.f,
        std::max(0.f, inner.width - 4.f),
        std::max(0.f, inner.height * 0.46f)
    };
    ren.drawRoundedRect(upper,
                        nxui::Color::white().withAlpha(0.020f * mat),
                        std::max(0.f, radius - 2.f));

    nxui::Rect lower = {
        inner.x + 2.f,
        inner.y + inner.height * 0.44f,
        std::max(0.f, inner.width - 4.f),
        std::max(0.f, inner.height * 0.52f)
    };
    ren.drawRoundedRect(lower,
                        nxui::Color::black().withAlpha(0.018f * mat),
                        std::max(0.f, radius - 2.f));

    ren.drawRoundedRectOutline(inner,
                               nxui::Color::white().withAlpha(0.105f * mat),
                               std::max(0.f, radius - 1.5f),
                               1.f);
    ren.drawRoundedRectOutline(rect.shrunk(2.8f),
                               nxui::Color::black().withAlpha(0.052f * mat),
                               std::max(0.f, radius - 2.8f),
                               1.f);

    nxui::Rect grain = rect.shrunk(6.f);
    int speckles = std::min(7, std::max(2, (int)(grain.width / 44.f)));
    for (int i = 0; i < speckles; ++i) {
        float t = (float)(i + 1) / (float)(speckles + 1);
        float x = grain.x + std::fmod((t * 37.0f + rect.x * 0.013f), 1.0f) * grain.width;
        float y = grain.y + std::fmod((t * 23.0f + rect.y * 0.017f), 1.0f) * grain.height;
        nxui::Color c = (i % 2 == 0)
            ? nxui::Color::white().withAlpha(0.024f * mat)
            : nxui::Color::black().withAlpha(0.018f * mat);
        ren.drawCircle({x, y}, 0.65f, c, 8);
    }
}

inline void drawActionButtonChrome(nxui::Renderer& ren,
                                   const nxui::Rect& rect,
                                   float radius,
                                   const ActionButtonVisualStyle& style) {
    nxui::Rect buttonRect = scaledActionButtonRect(rect, style.scale);
    radius = std::max(0.f, radius);

    if (style.outerGlowColor.a > 0.004f) {
        nxui::Rect contact = buttonRect;
        contact.y += 2.5f;
        ren.drawRoundedRect(contact.expanded(1.5f),
                            style.outerGlowColor,
                            radius + 1.5f);
    }

    ren.drawRoundedRect(buttonRect.expanded(0.8f),
                        nxui::Color::white().withAlpha(style.baseColor.a * 0.055f),
                        radius + 0.8f);

    ren.drawRoundedRect(buttonRect, style.baseColor, radius);
    drawActionButtonMaterial(ren, buttonRect, radius, style);

    if (style.highlightColor.a > 0.004f) {
        ren.drawRoundedRectOutline(buttonRect.shrunk(1.f),
                                   style.highlightColor,
                                   std::max(0.f, radius - 1.f), 1.f);
    }

    if (style.borderWidth > 0.f) {
        ren.drawRoundedRectOutline(buttonRect,
                                   style.borderColor,
                                   radius,
                                   style.borderWidth);
    }
}

} // namespace switchu::ui
