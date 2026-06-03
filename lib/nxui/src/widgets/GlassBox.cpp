#include <nxui/widgets/GlassBox.hpp>
#include <nxui/core/Renderer.hpp>
#include <algorithm>
#include <cmath>

namespace nxui {

void GlassBox::onRender(Renderer& ren) {
    if (m_scale < 0.01f) return;

    Rect r = m_rect;
    if (std::abs(m_scale - 1.f) > 0.001f) {
        float w = r.width  * m_scale;
        float h = r.height * m_scale;
        r.x += (r.width  - w) * 0.5f;
        r.y += (r.height - h) * 0.5f;
        r.width  = w;
        r.height = h;
    }

    float op = m_opacity * m_panelOpacity;
    float radius = std::max(0.f, m_radius);

    // Main translucent body.
    ren.drawRoundedRect(r, m_base.withAlpha(m_base.a * op), radius);

    // Subtle inner glow.
    Rect inner = r.shrunk(1.f);
    ren.drawRoundedRect(inner,
                        m_base.withAlpha(m_base.a * 0.25f * op),
                        std::max(0.f, radius - 1.f));

    if (m_highlight.a > 0.01f) {
        ren.drawRoundedRectOutline(r.shrunk(1.f),
                                   m_highlight.withAlpha(m_highlight.a * 0.45f * op),
                                   std::max(0.f, radius - 1.f), 1.f);
    }

    // Glossy rim.
    if (m_borderW > 0.f)
        ren.drawRoundedRectOutline(r, m_border.withAlpha(m_border.a * op),
                                   radius, m_borderW);

    // Box::onRender does nothing, but children are rendered by Widget::render
}

} // namespace nxui
