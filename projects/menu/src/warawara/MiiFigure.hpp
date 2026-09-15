#pragma once

#include "MiiAvatarManager.hpp"
#include "PlazaSpeechBubble.hpp"
#include <nxui/core/Types.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Font.hpp>

#include <cstdint>
#include <string>
#include <random>

namespace warawara {

/// High-level behavioral states for a Mii character in WaraWara Plaza.
enum class MiiState : uint8_t {
    Idle,   ///< Standing gently, breathing/bobbing, looking around.
    Walk,   ///< Moving towards target waypoint with sinusoidal foot/arm gait.
    Gather, ///< Clustered around a game pedestal or plaza center.
    Cheer,  ///< Jumping with joy, hands raised high, celebrating.
    Speak   ///< Conversational gesturing with speech bubble active.
};

/// Configuration parameters for Mii character rendering and physics.
struct MiiFigureConfig {
    float baseHeight = 76.0f;          ///< Base character height in pixels.
    float baseWidth = 44.0f;           ///< Base character width in pixels.
    float walkSpeed = 52.0f;           ///< Walking translation speed in pixels/second.
    float shadowOpacity = 0.32f;       ///< Ground drop-shadow base alpha.
    bool showNamePill = true;          ///< Whether to render nickname pill.
    bool namePillAbove = false;        ///< False = below feet; True = above head.
    nxui::Rect wanderBounds{80.0f, 400.0f, 1120.0f, 250.0f}; ///< Autonomous roaming bounds.
};

/// Represents an authentic animated 2D/2.5D Mii character in WaraWara Plaza.
/// Features procedural skeletal animation (alternating sinusoidal gait, swinging/raised hands,
/// favorite shirt color torso, rounded avatar head, soft floor drop-shadow, and name pill).
class MiiFigure {
public:
    explicit MiiFigure(const MiiAvatarData& data = {});
    ~MiiFigure() = default;

    /// Sets or updates the Mii avatar metadata (texture, name, colors, proportions).
    void setData(const MiiAvatarData& data);
    const MiiAvatarData& data() const { return m_data; }

    /// Configuration and bounds.
    void setConfig(const MiiFigureConfig& config) { m_config = config; }
    const MiiFigureConfig& config() const { return m_config; }
    MiiFigureConfig& config() { return m_config; }

    void setWanderBounds(const nxui::Rect& bounds) { m_config.wanderBounds = bounds; }
    const nxui::Rect& wanderBounds() const { return m_config.wanderBounds; }

    /// Ground anchor position (feet contact level).
    void setPosition(const nxui::Vec2& pos);
    const nxui::Vec2& position() const { return m_pos; }

    /// Scale multiplier (defaults to 1.0f).
    /// Clamped at both ends and NaN-rejected: an unbounded upper end let a
    /// caller's unsigned-underflow (2^64) reach the renderer and emit geometry
    /// with a +64 biased exponent. The comparison order also makes NaN fall
    /// through to the default rather than being stored.
    void setScale(float scale) {
        if (!(scale >= 0.1f)) {   // false for NaN and for values below 0.1
            m_scale = 0.1f;
        } else if (scale > 8.0f) {
            m_scale = 8.0f;
        } else {
            m_scale = scale;
        }
    }
    float scale() const { return m_scale; }

    /// State machine queries and triggers.
    MiiState state() const { return m_state; }
    void setState(MiiState state, float duration = 0.0f);

    /// Autonomous behavioral AI (wandering, gathering, cheering).
    void setAutonomous(bool enabled) { m_autonomous = enabled; }
    bool isAutonomous() const { return m_autonomous; }

    /// Direct action commands.
    void walkTo(const nxui::Vec2& destination);
    void gatherAt(const nxui::Vec2& center, float radius = 45.0f);
    void cheer(float duration = 2.8f);
    void speak(float duration = 4.5f);
    void idle(float duration = 0.0f);

    /// Speech bubble attachment
    void showSpeechBubble(const SpeechBubbleData& data, float duration = 6.5f);
    void dismissSpeechBubble(bool immediate = false);
    bool hasSpeechBubble() const { return m_speechBubble.isVisible(); }
    PlazaSpeechBubble& speechBubble() { return m_speechBubble; }
    const PlazaSpeechBubble& speechBubble() const { return m_speechBubble; }
    nxui::Vec2 headTopAnchor() const;

    /// Facing direction.
    bool isFacingLeft() const { return m_facingLeft; }
    void setFacingLeft(bool left) { m_facingLeft = left; }

    /// Hit testing for touch / pointer interaction.
    bool hitTest(const nxui::Vec2& point) const;

    /// Per-frame simulation update (dt in seconds).
    void update(float dt);

    /// Procedural skeletal rendering via nxui::Renderer.
    void render(nxui::Renderer& ren, nxui::Font* font = nullptr,
                nxui::Font* smallFont = nullptr, float cameraOffsetX = 0.0f,
                float viewZoom = 1.0f,
                const nxui::Vec2& viewCenter = {640.0f, 400.0f}) const;

private:
    void pickNewWanderTarget();
    void onStateTimerExpired();
    float randomFloat(float min, float max);

    MiiAvatarData       m_data;
    MiiFigureConfig     m_config;
    PlazaSpeechBubble   m_speechBubble;

    nxui::Vec2      m_pos{640.0f, 540.0f};      ///< Ground anchor point.
    nxui::Vec2      m_targetPos{640.0f, 540.0f};///< Navigation destination.
    nxui::Vec2      m_gatherCenter{640.0f, 540.0f}; ///< Focal point for Gather state.
    float           m_gatherRadius = 45.0f;

    float           m_scale = 1.0f;
    bool            m_facingLeft = false;
    bool            m_autonomous = true;

    MiiState        m_state = MiiState::Idle;
    float           m_stateTimer = 3.0f;        ///< Countdown timer for state transitions.
    float           m_animTime = 0.0f;          ///< Continuous animation clock.
    float           m_walkPhase = 0.0f;         ///< Sinusoidal walk cycle phase [0, 2*PI].

    mutable std::mt19937 m_rng{1337};
};

} // namespace warawara
