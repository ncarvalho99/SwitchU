#pragma once

#include <nxui/core/Types.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Texture.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace warawara {

/// Metadata and state for a community pedestal in WaraWara Plaza.
struct PlazaCommunityData {
    std::uint64_t titleId = 0;
    std::string titleName;
    std::string subtitle;
    nxui::Texture* iconTexture = nullptr;
    int gatheredMiiCount = 0;
};

/// Represents an authentic circular 2.5D community pedestal in WaraWara Plaza.
/// Features an elevated disc platform, glowing perimeter LED ring, floating bobbing game icon,
/// soft ground drop-shadow, and gathering waypoints for clustered Miis.
class PlazaPedestal {
public:
    PlazaPedestal(const PlazaCommunityData& data,
                  const nxui::Vec2& centerPos,
                  float baseRadius = 78.0f,
                  float scale = 1.0f);
    ~PlazaPedestal() = default;

    void update(float dt);
    void render(nxui::Renderer& ren, nxui::Font* font, nxui::Font* smallFont,
                float cameraOffsetX = 0.0f, float viewZoom = 1.0f,
                const nxui::Vec2& viewCenter = {640.0f, 400.0f}) const;

    bool hitTest(const nxui::Vec2& screenPoint, float cameraOffsetX = 0.0f,
                 float viewZoom = 1.0f,
                 const nxui::Vec2& viewCenter = {640.0f, 400.0f}) const;
    nxui::Rect bounds(float cameraOffsetX = 0.0f, float viewZoom = 1.0f,
                      const nxui::Vec2& viewCenter = {640.0f, 400.0f}) const;

    const PlazaCommunityData& data() const { return m_data; }
    void setData(const PlazaCommunityData& data) { m_data = data; }

    void setFocused(bool focused) { m_focused = focused; }
    bool isFocused() const { return m_focused; }

    const nxui::Vec2& position() const { return m_pos; }
    void setPosition(const nxui::Vec2& pos);

    float scale() const { return m_scale; }
    void setScale(float scale);

    /// Waypoints for Miis to stand naturally around the perimeter of the pedestal.
    nxui::Vec2 getGatheringSlot(std::size_t index) const;
    std::size_t gatheringSlotCount() const { return m_gatheringSlots.size(); }

private:
    void rebuildGatheringSlots();

    PlazaCommunityData m_data;
    nxui::Vec2 m_pos;
    float m_baseRadius = 78.0f;
    float m_scale = 1.0f;
    bool m_focused = false;
    float m_animTimer = 0.0f;
    float m_phase = 0.0f;
    std::vector<nxui::Vec2> m_gatheringSlots;
};

} // namespace warawara
