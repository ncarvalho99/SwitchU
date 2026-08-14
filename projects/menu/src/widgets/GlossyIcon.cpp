#include "GlossyIcon.hpp"
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Font.hpp>
#include <cmath>


GlossyIcon::GlossyIcon() {
    m_animScale.setImmediate(0.f);
    m_appearOpacity.setImmediate(0.f);
    m_focusScale.setImmediate(1.f);
    m_focusGlow.setImmediate(0.f);
    // Only a fallback: buildGrid overwrites this from Theme::iconCornerRadius
    // before anything is drawn, so it must not disagree with that default or it
    // describes a tile that never exists. Setting it here does not change the
    // rendered radius — that lives in the theme.
    setCornerRadius(24.f);
    setPadding(8.f);
    setLiquidGlassEnabled(true);
    setBlurEnabled(false);
}

void GlossyIcon::onFocusGained() {
    m_focused = true;
    m_focusScale.set(1.075f, 0.18f, nxui::Easing::outBack);
    m_focusGlow.set(1.f, 0.16f, nxui::Easing::outCubic);
}

void GlossyIcon::onFocusLost() {
    m_focused = false;
    m_focusScale.set(1.f, 0.20f, nxui::Easing::outCubic);
    m_focusGlow.set(0.f, 0.16f, nxui::Easing::outCubic);
}

void GlossyIcon::startAppear(float delay) {
    m_appearDelay = delay;
    m_appearTimer = 0.f;
    m_appearing   = true;
    m_animScale.setImmediate(0.f);
    m_appearOpacity.setImmediate(0.f);
}

void GlossyIcon::forceVisible() {
    m_appearing = false;
    m_appearDelay = 0.f;
    m_appearTimer = 0.f;
    m_animScale.setImmediate(1.f);
    m_appearOpacity.setImmediate(1.f);
}

void GlossyIcon::onContentUpdate(float dt) {
    if (m_appearing) {
        m_appearTimer += dt;
        if (m_appearTimer >= m_appearDelay) {
            m_appearing = false;
            m_animScale.set(1.f, 0.4f, nxui::Easing::outExpo);
            m_appearOpacity.set(1.f, 0.3f, nxui::Easing::outExpo);
        }
    }
    m_suspendPulse += dt * 2.2f;
}

void GlossyIcon::onRender(nxui::Renderer& ren) {
    float externalScale = scale();
    float focusS = m_focusScale.value();
    float s = m_animScale.value() * externalScale * focusS;
    float a = m_appearOpacity.value();
    if (s < 0.01f || a < 0.01f) return;

    nxui::Rect savedRect = m_rect;
    nxui::Rect drawRect = savedRect;
    if (std::abs(s - 1.f) > 0.001f) {
        float w = savedRect.width * s;
        float h = savedRect.height * s;
        drawRect.x += (savedRect.width - w) * 0.5f;
        drawRect.y += (savedRect.height - h) * 0.5f;
        drawRect.width = w;
        drawRect.height = h;
        m_rect = drawRect;
    }
    setScale(1.f);
    float savedShade = liquidGlassShade();
    setLiquidGlassShade(m_notLaunchable ? 0.58f : 0.0f);

    float savedOp = m_opacity;
    m_opacity = a * savedOp;

    nxui::GlassWidget::onRender(ren);

    m_opacity = savedOp;
    m_rect = savedRect;
    setLiquidGlassShade(savedShade);
    setScale(externalScale);

    nxui::Rect r = drawRect;
    float rad = cornerRadius();

    float focusGlow = m_focusGlow.value();
    if (focusGlow > 0.01f && s > 0.5f) {
        float breathe = 0.5f + 0.5f * std::sin(m_suspendPulse * 1.8f + 0.4f);
        nxui::Color focusColor = nxui::Color(0.65f, 0.90f, 1.f, (0.08f + 0.04f * breathe) * focusGlow * a);
        ren.drawRoundedRect(r.expanded(7.f * focusGlow), focusColor, rad + 7.f);
    }

    if (m_isGameCard && !m_notLaunchable && s > 0.5f) {
        float badgeW = 66.f * s;
        float badgeH = 48.f * s;
        float badgeX = r.x + 1.f * s;
        float badgeY = r.y + 6.f * s;

        if (m_gameCardTex && m_gameCardTex->valid()) {
            float cardInset = 1.f * s;
            float maxW = badgeW - cardInset * 2;
            float maxH = badgeH - cardInset * 2;
            float aspect = (float)m_gameCardTex->width() / (float)m_gameCardTex->height();
            float texW = maxW;
            float texH = maxH;
            if (aspect > maxW / maxH) {
                texH = maxW / aspect;
            } else {
                texW = maxH * aspect;
            }
            float texX = badgeX + (badgeW - texW) * 0.5f;
            float texY = badgeY + (badgeH - texH) * 0.5f;
            ren.drawTextureRounded(m_gameCardTex, {texX, texY, texW, texH}, 2.f * s,
                                   nxui::Color::white().withAlpha(0.98f * a));
        } else {
            float cardInset = 4.f * s;
            ren.drawRoundedRect({badgeX + cardInset, badgeY + cardInset,
                                 badgeW - cardInset*2, badgeH - cardInset*2},
                                nxui::Color(0.95f, 0.75f, 0.2f, 0.9f * a), 2.f * s);
        }
    }

    if (m_suspended && s > 0.5f) {
        float pulse = 0.5f + 0.5f * std::sin(m_suspendPulse);
        float glowAlpha = 0.35f + 0.25f * pulse;

        nxui::Color glow(0.18f, 0.85f, 0.45f, glowAlpha * a);
        ren.drawRoundedRectOutline(r.expanded(2.f), glow, rad + 2.f, 2.5f);

        float badgeSize = 26.f * s;
        float badgeX = r.x + r.width  - badgeSize - 4.f * s;
        float badgeY = r.y + r.height - badgeSize - 4.f * s;

        nxui::Vec2 badgeCenter = { badgeX + badgeSize * 0.5f, badgeY + badgeSize * 0.5f };
        ren.drawCircle(badgeCenter, badgeSize * 0.5f,
                       nxui::Color(0.1f, 0.1f, 0.1f, 0.85f * a), 16);

        float triH = badgeSize * 0.45f;
        float triW = triH * 0.85f;
        nxui::Vec2 p1 = { badgeCenter.x - triW * 0.35f, badgeCenter.y - triH * 0.5f };
        nxui::Vec2 p2 = { badgeCenter.x - triW * 0.35f, badgeCenter.y + triH * 0.5f };
        nxui::Vec2 p3 = { badgeCenter.x + triW * 0.65f, badgeCenter.y };
        ren.drawTriangle(p1, p2, p3, nxui::Color(0.18f, 0.85f, 0.45f, 0.95f * a));
    }
}

void GlossyIcon::onContentRender(nxui::Renderer& ren) {
    if (!m_tex || !m_tex->valid()) return;

    float s = scale();
    float rad = cornerRadius();

    nxui::Rect r = m_rect;
    if (s < 1.f) {
        float w = r.width  * s;
        float h = r.height * s;
        r.x += (r.width  - w) * 0.5f;
        r.y += (r.height - h) * 0.5f;
        r.width  = w;
        r.height = h;
    }

    float inset = 8.f * s;
    nxui::Rect texRect = r.shrunk(inset);
    nxui::Color iconTint = nxui::Color::white().withAlpha(m_opacity);
    if (m_notLaunchable) {
        iconTint.r = 0.80f;
        iconTint.g = 0.80f;
        iconTint.b = 0.80f;
    }
    // SteamGridDB covers are normally portrait while a Switch home icon is
    // square. Fill the native icon shape with a centered crop: this preserves
    // the image proportions and avoids both the squeezed look and side bars.
    if (m_customArtwork && m_tex->height() > 0) {
        nxui::Rect source{0.f, 0.f, (float)m_tex->width(), (float)m_tex->height()};
        const float artworkAspect = source.width / source.height;
        const float destinationAspect = texRect.width / texRect.height;
        if (artworkAspect < destinationAspect) {
            source.height = source.width / destinationAspect;
            source.y = ((float)m_tex->height() - source.height) * 0.5f;
        } else if (artworkAspect > destinationAspect) {
            source.width = source.height * destinationAspect;
            source.x = ((float)m_tex->width() - source.width) * 0.5f;
        }
        ren.drawTextureRoundedSub(m_tex, source, texRect, rad - 3.f, iconTint);
        return;
    }
    // rad - 3, not rad - inset. Matching the inset would make the bezel a
    // constant width, but it also drops the artwork to radius 16 against a
    // selection ring at 26, and the two then read as different shapes. Keeping
    // the artwork at 21 is the author's proportion and the one that looks
    // right; the bezel thickening slightly through the corner is the price.
    ren.drawTextureRounded(m_tex, texRect, rad - 3.f, iconTint);
}
