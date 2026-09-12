#include "PlazaSpeechBubble.hpp"

#include <cmath>
#include <sstream>
#include <algorithm>

namespace warawara {

void PlazaSpeechBubble::show(const SpeechBubbleData& data, const nxui::Vec2& anchorPos, float duration) {
    m_data = data;
    m_data.duration = duration;
    m_anchor = anchorPos;
    m_timer = duration;
    m_lifeTime = 0.0f;
    m_animProgress = 0.0f;
    m_visible = true;
    m_dismissing = false;
    m_layoutDirty = true;
}

void PlazaSpeechBubble::dismiss(bool immediate) {
    if (!m_visible) return;

    if (immediate) {
        m_visible = false;
        m_animProgress = 0.0f;
    } else {
        m_dismissing = true;
    }
}

void PlazaSpeechBubble::setAnchor(const nxui::Vec2& anchor) {
    m_anchor = anchor;
    m_layoutDirty = true;
}

bool PlazaSpeechBubble::hitTest(const nxui::Vec2& point) const {
    if (!m_visible) return false;
    return m_bounds.contains(point);
}

bool PlazaSpeechBubble::hitTestYeah(const nxui::Vec2& point) const {
    if (!m_visible) return false;
    return m_yeahRect.contains(point);
}

void PlazaSpeechBubble::giveYeah() {
    if (!m_visible || m_data.hasYead) return;
    m_data.hasYead = true;
    m_data.yeahCount++;
    // Extend display timer slightly on positive user interaction
    m_timer = std::max(m_timer, 3.5f);
}

void PlazaSpeechBubble::update(float dt) {
    if (!m_visible) return;

    m_lifeTime += dt;

    if (m_dismissing) {
        m_animProgress -= dt * 5.0f; // Rapid 0.2s fade-out
        if (m_animProgress <= 0.0f) {
            m_animProgress = 0.0f;
            m_visible = false;
            m_dismissing = false;
        }
        return;
    }

    // Pop-in animation: 0.0 -> 1.0 over ~0.22 seconds
    if (m_animProgress < 1.0f) {
        m_animProgress += dt * 4.5f;
        if (m_animProgress > 1.0f) m_animProgress = 1.0f;
    }

    if (m_timer > 0.0f) {
        m_timer -= dt;
        if (m_timer <= 0.0f) {
            dismiss(false);
        }
    }
}

std::vector<std::string> PlazaSpeechBubble::wrapText(nxui::Font* font, const std::string& text,
                                                    float maxWidth, float scale) const {
    std::vector<std::string> lines;
    if (text.empty()) return lines;

    std::istringstream stream(text);
    std::string word;
    std::string currentLine;

    while (stream >> word) {
        std::string testLine = currentLine.empty() ? word : (currentLine + " " + word);
        nxui::Vec2 sz = font ? font->measure(testLine) : nxui::Vec2{static_cast<float>(testLine.size()) * 7.0f, 14.0f};
        float testWidth = sz.x * scale;

        if (testWidth > maxWidth && !currentLine.empty()) {
            lines.push_back(currentLine);
            currentLine = word;
            if (lines.size() >= 3) {
                // Truncate overflow gracefully with ellipsis
                currentLine += "…";
                break;
            }
        } else {
            currentLine = testLine;
        }
    }

    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    return lines;
}

void PlazaSpeechBubble::updateLayout(nxui::Font* font, nxui::Font* smallFont) {
    const float bubbleWidth = 260.0f;
    const float contentWidth = bubbleWidth - 28.0f;
    const float fontScale = 0.58f;

    m_wrappedLines = wrapText(smallFont ? smallFont : font, m_data.text, contentWidth, fontScale);

    float lineHeight = 17.0f;
    float textBlockHeight = static_cast<float>(m_wrappedLines.size()) * lineHeight;
    if (textBlockHeight < 20.0f) textBlockHeight = 20.0f;

    float headerHeight = (!m_data.author.empty() || !m_data.topic.empty()) ? 20.0f : 4.0f;
    float footerHeight = 24.0f;
    float bubbleHeight = headerHeight + textBlockHeight + footerHeight + 12.0f;

    // Determine vertical placement (prefer above Mii, flip below if too close to screen top)
    float floatOffset = std::sin(m_lifeTime * 2.5f) * 1.5f;
    float bubbleY = m_anchor.y - bubbleHeight - 12.0f + floatOffset;
    m_tailDir = BubbleTailDirection::Down;

    if (bubbleY < 24.0f) {
        bubbleY = m_anchor.y + 16.0f + floatOffset;
        m_tailDir = BubbleTailDirection::Up;
    }

    // Determine horizontal placement (clamped to screen boundaries with padding)
    float bubbleX = m_anchor.x - bubbleWidth * 0.5f;
    bubbleX = std::clamp(bubbleX, 16.0f, 1280.0f - bubbleWidth - 16.0f);

    m_bounds = {bubbleX, bubbleY, bubbleWidth, bubbleHeight};

    // "Yeah!" button bounds in bottom right
    float yeahWidth = 62.0f;
    float yeahHeight = 20.0f;
    m_yeahRect = {
        bubbleX + bubbleWidth - yeahWidth - 12.0f,
        bubbleY + bubbleHeight - yeahHeight - 8.0f,
        yeahWidth,
        yeahHeight
    };

    // Calculate tail geometry connecting to anchor
    float tailBaseX = std::clamp(m_anchor.x, bubbleX + 22.0f, bubbleX + bubbleWidth - 22.0f);
    if (m_tailDir == BubbleTailDirection::Down) {
        float baseY = bubbleY + bubbleHeight;
        m_tailBaseLeft = {tailBaseX - 8.0f, baseY};
        m_tailBaseRight = {tailBaseX + 8.0f, baseY};
        m_tailTip = {m_anchor.x, baseY + 10.0f};
    } else {
        float baseY = bubbleY;
        m_tailBaseLeft = {tailBaseX - 8.0f, baseY};
        m_tailBaseRight = {tailBaseX + 8.0f, baseY};
        m_tailTip = {m_anchor.x, baseY - 10.0f};
    }

    m_layoutDirty = false;
}

void PlazaSpeechBubble::render(nxui::Renderer& ren, nxui::Font* font, nxui::Font* smallFont) const {
    if (!m_visible || m_animProgress <= 0.001f) return;

    if (m_layoutDirty) {
        const_cast<PlazaSpeechBubble*>(this)->updateLayout(font, smallFont);
    }

    float alpha = std::min(1.0f, m_animProgress * 1.3f);
    float scale = 0.85f + 0.15f * m_animProgress;

    // Center of bubble for pop scale effect
    nxui::Vec2 center{m_bounds.x + m_bounds.width * 0.5f, m_bounds.y + m_bounds.height * 0.5f};
    float w = m_bounds.width * scale;
    float h = m_bounds.height * scale;
    nxui::Rect renderRect{center.x - w * 0.5f, center.y - h * 0.5f, w, h};
    float radius = 13.0f * scale;

    // 1. Soft Drop-Shadow
    nxui::Rect shadowRect{renderRect.x + 2.0f, renderRect.y + 3.0f, renderRect.width, renderRect.height};
    ren.drawRoundedRect(shadowRect, nxui::Color(0.0f, 0.0f, 0.0f, 0.15f * alpha), radius);

    nxui::Vec2 shadowTailTip{m_tailTip.x + 2.0f, m_tailTip.y + 3.0f};
    nxui::Vec2 shadowBaseL{m_tailBaseLeft.x + 2.0f, m_tailBaseLeft.y + 3.0f};
    nxui::Vec2 shadowBaseR{m_tailBaseRight.x + 2.0f, m_tailBaseRight.y + 3.0f};
    ren.drawTriangle(shadowBaseL, shadowBaseR, shadowTailTip, nxui::Color(0.0f, 0.0f, 0.0f, 0.15f * alpha));

    // 2. Bubble Body (Wii U clean white / off-white)
    nxui::Color bubbleBg(0.98f, 0.98f, 1.0f, 0.96f * alpha);
    ren.drawRoundedRect(renderRect, bubbleBg, radius);

    // 3. Pointer Tail Fill
    ren.drawTriangle(m_tailBaseLeft, m_tailBaseRight, m_tailTip, bubbleBg);

    // 4. Outlines (Bubble and Tail)
    nxui::Color outlineColor(0.72f, 0.76f, 0.82f, 0.75f * alpha);
    ren.drawRoundedRectOutline(renderRect, outlineColor, radius, 1.2f);
    ren.drawLine(m_tailBaseLeft, m_tailTip, outlineColor, 1.2f);
    ren.drawLine(m_tailBaseRight, m_tailTip, outlineColor, 1.2f);

    // 5. Header: Author & Topic
    nxui::Font* textFont = smallFont ? smallFont : font;
    float textScale = 0.56f * scale;
    float curY = renderRect.y + 10.0f * scale;
    float curX = renderRect.x + 14.0f * scale;

    if (!m_data.author.empty() && textFont) {
        nxui::Color authorColor(0.18f, 0.35f, 0.65f, 0.95f * alpha);
        ren.drawText(m_data.author, {curX, curY}, textFont, authorColor, textScale);

        if (!m_data.topic.empty()) {
            nxui::Vec2 authSz = textFont->measure(m_data.author);
            float topicX = curX + (authSz.x * textScale) + 8.0f * scale;
            nxui::Color topicColor(0.48f, 0.52f, 0.58f, 0.85f * alpha);
            ren.drawText("• " + m_data.topic, {topicX, curY}, textFont, topicColor, textScale * 0.92f);
        }
        curY += 18.0f * scale;
    }

    // 6. Body Text Lines
    if (textFont) {
        nxui::Color textColor(0.12f, 0.14f, 0.18f, 0.95f * alpha);
        for (const auto& line : m_wrappedLines) {
            ren.drawText(line, {curX, curY}, textFont, textColor, textScale);
            curY += 17.0f * scale;
        }
    }

    // 7. Interactive Miiverse "Yeah!" Button
    nxui::Color yeahBg = m_data.hasYead
        ? nxui::Color(1.0f, 0.62f, 0.12f, 0.92f * alpha)  // Cheerful active orange
        : nxui::Color(0.90f, 0.92f, 0.95f, 0.85f * alpha); // Neutral pill

    ren.drawRoundedRect(m_yeahRect, yeahBg, m_yeahRect.height * 0.5f);
    ren.drawRoundedRectOutline(m_yeahRect, nxui::Color(0.78f, 0.82f, 0.88f, 0.80f * alpha),
                               m_yeahRect.height * 0.5f, 1.0f);

    if (textFont) {
        std::string yeahLabel = (m_data.hasYead ? "★ Yeah! " : "Yeah! ") + std::to_string(m_data.yeahCount);
        nxui::Color yeahTextColor = m_data.hasYead
            ? nxui::Color(1.0f, 1.0f, 1.0f, alpha)
            : nxui::Color(0.35f, 0.40f, 0.48f, alpha);

        nxui::Vec2 ySz = textFont->measure(yeahLabel);
        float yTextScale = 0.48f * scale;
        nxui::Vec2 yPos{
            m_yeahRect.x + (m_yeahRect.width - ySz.x * yTextScale) * 0.5f,
            m_yeahRect.y + (m_yeahRect.height - ySz.y * yTextScale) * 0.5f - 1.0f * scale
        };
        ren.drawText(yeahLabel, yPos, textFont, yeahTextColor, yTextScale);
    }
}

} // namespace warawara
