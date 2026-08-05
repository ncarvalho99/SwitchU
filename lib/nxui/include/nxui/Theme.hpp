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
    // The cursor sits on the icon rect grown by 4, so its arc only shares a
    // centre with the icon's at iconCornerRadius + 4. At 26 it did not, and the
    // selection ring visibly disagreed with the tile it was framing.
    float cursorCornerRadius = 28.f;
    float cursorBorderWidth  = 3.f;
    float panelCornerRadius  = 24.f;
    float cellCornerRadius   = 20.f;

    static Theme dark();
    static Theme light();
};

} // namespace nxui
