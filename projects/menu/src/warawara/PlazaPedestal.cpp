#include "PlazaPedestal.hpp"
#include <cmath>
#include <algorithm>

namespace warawara {

PlazaPedestal::PlazaPedestal(const PlazaCommunityData& data,
                             const nxui::Vec2& centerPos,
                             float baseRadius,
                             float scale)
    : m_data(data)
    , m_pos(centerPos)
    , m_baseRadius(baseRadius)
    , m_scale(scale)
{
    // Stagger float animation phase based on position to avoid uniform oscillation
    m_phase = std::fmod((centerPos.x * 0.013f + centerPos.y * 0.027f), 6.283185f);
    rebuildGatheringSlots();
}

void PlazaPedestal::setPosition(const nxui::Vec2& pos) {
    m_pos = pos;
    rebuildGatheringSlots();
}

void PlazaPedestal::setScale(float scale) {
    m_scale = (scale < 0.1f) ? 0.1f : scale;
    rebuildGatheringSlots();
}

void PlazaPedestal::rebuildGatheringSlots() {
    m_gatheringSlots.clear();
    // 5 natural gathering waypoints around the sides and front of the pedestal
    const float effRadius = (m_baseRadius + 26.0f) * m_scale;
    const float angles[] = {
        -2.50f, // Left rear
        -1.75f, // Left front
         0.00f, // Direct front
         1.75f, // Right front
         2.50f  // Right rear
    };

    for (float ang : angles) {
        float x = m_pos.x + std::sin(ang) * effRadius;
        // Perspective foreshortening on Y axis for 2.5D disc floor
        float y = m_pos.y + std::cos(ang) * (effRadius * 0.52f) + (14.0f * m_scale);
        m_gatheringSlots.push_back({x, y});
    }
}

nxui::Vec2 PlazaPedestal::getGatheringSlot(std::size_t index) const {
    if (m_gatheringSlots.empty()) {
        return m_pos;
    }
    return m_gatheringSlots[index % m_gatheringSlots.size()];
}

void PlazaPedestal::update(float dt) {
    m_animTimer += dt;
}

void PlazaPedestal::render(nxui::Renderer& ren,
                           nxui::Font* font,
                           nxui::Font* smallFont,
                           float cameraOffsetX,
                           float viewZoom,
                           const nxui::Vec2& viewCenter) const {
    const float sx = viewCenter.x + (m_pos.x - cameraOffsetX - viewCenter.x) * viewZoom;
    const float sy = viewCenter.y + (m_pos.y - viewCenter.y) * viewZoom;

    const float objectScale = m_scale * viewZoom;
    const float effR = m_baseRadius * objectScale;

    // Viewport culling (screen 1280x720)
    if (sx < -effR * 2.0f || sx > 1280.0f + effR * 2.0f) {
        return;
    }

    // 1. Soft Floor Drop-Shadow (elliptical shadow on plaza floor)
    const float shadowW = (effR + 18.0f * objectScale) * 2.0f;
    const float shadowH = effR * 0.72f;
    nxui::Rect shadowRect{sx - shadowW * 0.5f, sy - shadowH * 0.25f, shadowW, shadowH};
    ren.drawRoundedRect(shadowRect, nxui::Color(0.04f, 0.07f, 0.14f, 0.32f), shadowH * 0.5f);

    // 2. Pedestal 3D Base Platform (cylinder bevel edge)
    const float baseW = effR * 2.0f;
    const float baseH = effR * 0.60f;
    nxui::Rect bevelRect{sx - baseW * 0.5f, sy - baseH * 0.35f, baseW, baseH};
    ren.drawRoundedRect(bevelRect, nxui::Color(0.70f, 0.76f, 0.84f, 0.88f), baseH * 0.5f);

    // 3. Pedestal Top Disc Surface
    const float topW = (effR - 4.0f * objectScale) * 2.0f;
    const float topH = (effR - 4.0f * objectScale) * 0.58f;
    nxui::Rect topRect{sx - topW * 0.5f, sy - topH * 0.42f - 6.0f * objectScale, topW, topH};
    ren.drawRoundedRect(topRect, nxui::Color(0.92f, 0.95f, 0.98f, 0.96f), topH * 0.5f);

    // 4. Perimeter LED Glowing Ring
    const float pulse = 0.5f + 0.5f * std::sin(m_animTimer * 2.5f + m_phase);
    nxui::Color ringCol = m_focused
        ? nxui::Color(1.0f, 0.84f, 0.22f, 0.85f + 0.15f * pulse)
        : nxui::Color(0.22f, 0.68f, 0.98f, 0.65f + 0.20f * pulse);

    ren.drawRoundedRectOutline(topRect, ringCol, topH * 0.5f, m_focused ? (3.0f * objectScale) : (1.8f * objectScale));

    // 5. Floating Game Icon (with sinusoidal bobbing elevation)
    const float bobOffset = std::sin(m_animTimer * 2.0f + m_phase) * (4.5f * objectScale);
    const float iconSize = 72.0f * objectScale;
    const float iconCX = sx;
    const float iconCY = sy - (54.0f * objectScale) + bobOffset;
    nxui::Rect iconRect{iconCX - iconSize * 0.5f, iconCY - iconSize * 0.5f, iconSize, iconSize};

    // Soft drop shadow cast by the floating icon onto the pedestal platform
    nxui::Rect iconShadowRect{
        iconCX - iconSize * 0.40f,
        topRect.y + topRect.height * 0.45f,
        iconSize * 0.80f,
        12.0f * objectScale
    };
    ren.drawRoundedRect(iconShadowRect, nxui::Color(0.04f, 0.08f, 0.16f, 0.36f), iconShadowRect.height * 0.5f);

    // Draw game icon
    const float iconRadius = 16.0f * objectScale;
    if (m_data.iconTexture) {
        ren.drawTextureRounded(m_data.iconTexture, iconRect, iconRadius);
    } else {
        // High quality fallback community tile
        nxui::Color tileCol(0.18f, 0.38f, 0.72f, 0.95f);
        ren.drawRoundedRect(iconRect, tileCol, iconRadius);
        // Initial letter placeholder
        if (font && !m_data.titleName.empty()) {
            std::string initial = m_data.titleName.substr(0, 1);
            nxui::Vec2 textSize = font->measure(initial);
            float tScale = 0.90f * objectScale;
            nxui::Vec2 textPos{
                iconCX - (textSize.x * tScale) * 0.5f,
                iconCY - (textSize.y * tScale) * 0.5f
            };
            ren.drawText(initial, textPos, font, nxui::Color::white(), tScale);
        }
    }

    // Icon border glow
    nxui::Color iconBorderCol = m_focused
        ? nxui::Color(1.0f, 0.86f, 0.25f, 0.95f)
        : nxui::Color(1.0f, 1.0f, 1.0f, 0.75f);
    ren.drawRoundedRectOutline(iconRect, iconBorderCol, iconRadius, m_focused ? (2.8f * objectScale) : (1.4f * objectScale));

    // 6. Title Pill & Subtitle
    if (font && !m_data.titleName.empty()) {
        const float fontScale = 0.52f * objectScale;
        std::string displayTitle = m_data.titleName;
        if (displayTitle.length() > 22) {
            displayTitle = displayTitle.substr(0, 20) + "...";
        }
        nxui::Vec2 titleSize = font->measure(displayTitle);

        float pillW = std::max(titleSize.x * fontScale + 20.0f * objectScale, 100.0f * objectScale);
        float pillH = 22.0f * objectScale;
        float pillX = sx - pillW * 0.5f;
        float pillY = iconCY + iconSize * 0.5f + 6.0f * objectScale;
        nxui::Rect pillRect{pillX, pillY, pillW, pillH};

        // Frosted glass background pill
        nxui::Color pillBg = m_focused
            ? nxui::Color(0.08f, 0.16f, 0.28f, 0.88f)
            : nxui::Color(0.05f, 0.08f, 0.14f, 0.78f);
        ren.drawRoundedRect(pillRect, pillBg, pillH * 0.5f);

        nxui::Color pillOutline = m_focused
            ? nxui::Color(1.0f, 0.85f, 0.25f, 0.80f)
            : nxui::Color(1.0f, 1.0f, 1.0f, 0.25f);
        ren.drawRoundedRectOutline(pillRect, pillOutline, pillH * 0.5f, 1.0f);

        nxui::Vec2 textPos{
            pillX + (pillW - titleSize.x * fontScale) * 0.5f,
            pillY + (pillH - titleSize.y * fontScale) * 0.5f - 1.0f * objectScale
        };
        ren.drawText(displayTitle, textPos, font, nxui::Color(0.96f, 0.97f, 0.99f), fontScale);

        // Optional subtitle below title pill
        if (smallFont && !m_data.subtitle.empty()) {
            const float subScale = 0.44f * objectScale;
            nxui::Vec2 subSize = smallFont->measure(m_data.subtitle);
            nxui::Vec2 subPos{
                sx - (subSize.x * subScale) * 0.5f,
                pillY + pillH + 3.0f * objectScale
            };
            ren.drawText(m_data.subtitle, subPos, smallFont, nxui::Color(0.24f, 0.68f, 0.98f, 0.90f), subScale);
        }
    }
}

bool PlazaPedestal::hitTest(const nxui::Vec2& screenPoint, float cameraOffsetX,
                            float viewZoom, const nxui::Vec2& viewCenter) const {
    nxui::Rect b = bounds(cameraOffsetX, viewZoom, viewCenter);
    return b.contains(screenPoint.x, screenPoint.y);
}

nxui::Rect PlazaPedestal::bounds(float cameraOffsetX, float viewZoom,
                                 const nxui::Vec2& viewCenter) const {
    const float objectScale = m_scale * viewZoom;
    const float sx = viewCenter.x + (m_pos.x - cameraOffsetX - viewCenter.x) * viewZoom;
    const float sy = viewCenter.y + (m_pos.y - viewCenter.y) * viewZoom;
    const float effR = (m_baseRadius + 10.0f) * objectScale;
    const float topY = sy - 95.0f * objectScale;
    const float bottomY = sy + effR * 0.45f;
    return nxui::Rect{sx - effR, topY, effR * 2.0f, bottomY - topY};
}

} // namespace warawara
