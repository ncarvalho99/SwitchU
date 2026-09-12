#pragma once

#include <nxui/core/Types.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Font.hpp>

#include <string>
#include <vector>

namespace warawara {

enum class BubbleTailDirection : uint8_t {
    Down,
    Up
};

/// Data container for a WaraWara Plaza speech bubble post or tip.
struct SpeechBubbleData {
    std::string text;
    std::string author = "Mii";
    std::string topic;              ///< Optional category, game name, or tag.
    int yeahCount = 0;             ///< Authentic Miiverse "Yeah!" counter.
    bool hasYead = false;          ///< User has given a "Yeah!" to this post.
    float duration = 6.5f;         ///< Auto-dismiss timeout in seconds.
};

/// Wii U style rounded rectangle speech bubble with triangular pointer tail,
/// soft drop-shadow, multi-line text wrapping, and interactive Miiverse "Yeah!" button.
class PlazaSpeechBubble {
public:
    PlazaSpeechBubble() = default;
    ~PlazaSpeechBubble() = default;

    /// Shows the speech bubble anchored to a world position (e.g. Mii head top).
    void show(const SpeechBubbleData& data, const nxui::Vec2& anchorPos, float duration = 6.5f);

    /// Dismisses the speech bubble (fade out or immediate).
    void dismiss(bool immediate = false);

    /// Whether the speech bubble is currently displayed.
    bool isVisible() const { return m_visible; }

    /// Associated data and anchor.
    const SpeechBubbleData& data() const { return m_data; }
    const nxui::Vec2& anchor() const { return m_anchor; }
    void setAnchor(const nxui::Vec2& anchor);

    /// Hit testing for touch interaction (taps on the bubble or "Yeah!" button).
    bool hitTest(const nxui::Vec2& point) const;
    bool hitTestYeah(const nxui::Vec2& point) const;

    /// Triggers a "Yeah!" reaction on the bubble.
    void giveYeah();

    /// Per-frame animation and timer update.
    void update(float dt);

    /// Renders the bubble via nxui::Renderer.
    void render(nxui::Renderer& ren, nxui::Font* font, nxui::Font* smallFont = nullptr) const;

private:
    void updateLayout(nxui::Font* font, nxui::Font* smallFont);
    std::vector<std::string> wrapText(nxui::Font* font, const std::string& text, float maxWidth, float scale) const;

    SpeechBubbleData m_data;
    nxui::Vec2       m_anchor{640.f, 360.f};

    // Computed geometry
    mutable nxui::Rect m_bounds{0.f, 0.f, 0.f, 0.f};
    mutable nxui::Rect m_yeahRect{0.f, 0.f, 0.f, 0.f};
    mutable nxui::Vec2 m_tailTip{0.f, 0.f};
    mutable nxui::Vec2 m_tailBaseLeft{0.f, 0.f};
    mutable nxui::Vec2 m_tailBaseRight{0.f, 0.f};
    mutable BubbleTailDirection m_tailDir = BubbleTailDirection::Down;
    mutable std::vector<std::string> m_wrappedLines;
    mutable bool m_layoutDirty = true;

    float m_timer = 0.0f;
    float m_animProgress = 0.0f;    ///< 0.0 to 1.0
    float m_lifeTime = 0.0f;
    bool  m_visible = false;
    bool  m_dismissing = false;
};

} // namespace warawara
