#pragma once
#include <nxui/core/Types.hpp>

namespace nxui {

enum class ThemeMode { Dark, Light };

struct Theme {
    ThemeMode mode = ThemeMode::Dark;

    // Background
    Color background;
    Color backgroundAccent;   // gradient / secondary bg tint

    // Glass panels
    Color panelBase;
    Color panelBorder;
    Color panelHighlight;     // top shine

    // Icons
    Color iconDefault;

    // Cursor
    Color cursorNormal;
    Color cursorGlow;

    // Text
    Color textPrimary;
    Color textSecondary;

    // Page indicator dots
    Color pageIndicator;
    Color pageIndicatorActive;

    // Animated background shapes
    Color shapeColor;

    // Radii & sizes
    float iconCornerRadius   = 24.f;
    // 26, not the 28 that would be concentric with the icon rect grown by 4.
    // Concentric is geometrically right and looked wrong: it rounds the ring
    // further from the artwork it frames. Symmetry between the two won.
    float cursorCornerRadius = 26.f;
    float cursorBorderWidth  = 3.f;
    float panelCornerRadius  = 24.f;
    float cellCornerRadius   = 20.f;

    static Theme dark();
    static Theme light();
};

} // namespace nxui
