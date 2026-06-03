#pragma once

#include "ActionButtonStyle.hpp"
#include <nxui/widgets/GlassPanel.hpp>
#include <nxui/Theme.hpp>

class ActionButton : public nxui::GlassPanel {
public:
    ActionButton();

    void setTheme(const nxui::Theme* theme) { m_theme = theme; }
    void setVisualState(float opacity, float emphasis, float accentMix = -1.f);

protected:
    void onRender(nxui::Renderer& ren) override;

private:
    const nxui::Theme* m_theme = nullptr;
    float m_styleOpacity = 1.f;
    float m_emphasis = 0.f;
    float m_accentMix = -1.f;
};
