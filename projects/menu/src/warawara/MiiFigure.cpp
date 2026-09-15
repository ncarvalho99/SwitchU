#include "MiiFigure.hpp"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace warawara {

MiiFigure::MiiFigure(const MiiAvatarData& data)
    : m_data(data) {
    std::random_device rd;
    m_rng.seed(rd());
    m_stateTimer = randomFloat(2.0f, 5.0f);
}

void MiiFigure::setData(const MiiAvatarData& data) {
    m_data = data;
}

void MiiFigure::setPosition(const nxui::Vec2& pos) {
    m_pos = pos;
    m_targetPos = pos;
}

void MiiFigure::setState(MiiState state, float duration) {
    m_state = state;
    if (duration > 0.0f) {
        m_stateTimer = duration;
    } else {
        switch (state) {
        case MiiState::Idle:
            m_stateTimer = randomFloat(2.0f, 5.0f);
            break;
        case MiiState::Walk:
            m_stateTimer = 15.0f; // Walk timeout fallback
            break;
        case MiiState::Gather:
            m_stateTimer = randomFloat(6.0f, 12.0f);
            break;
        case MiiState::Cheer:
            m_stateTimer = randomFloat(2.0f, 3.2f);
            break;
        case MiiState::Speak:
            m_stateTimer = randomFloat(3.5f, 5.5f);
            break;
        }
    }
}

void MiiFigure::walkTo(const nxui::Vec2& destination) {
    nxui::Vec2 clamped = destination;
    if (m_config.wanderBounds.width > 0.0f && m_config.wanderBounds.height > 0.0f) {
        clamped.x = std::clamp(clamped.x, m_config.wanderBounds.x, m_config.wanderBounds.right());
        clamped.y = std::clamp(clamped.y, m_config.wanderBounds.y, m_config.wanderBounds.bottom());
    }

    m_targetPos = clamped;
    m_facingLeft = (m_targetPos.x < m_pos.x);
    setState(MiiState::Walk, 14.0f);
}

void MiiFigure::gatherAt(const nxui::Vec2& center, float radius) {
    m_gatherCenter = center;
    m_gatherRadius = radius;

    // Pick a point clustered near the focal center with perspective compression
    float angle = randomFloat(0.0f, 6.2831853f);
    float dist = randomFloat(radius * 0.3f, radius);
    nxui::Vec2 target = center + nxui::Vec2{std::cos(angle) * dist, std::sin(angle) * dist * 0.5f};

    walkTo(target);
}

void MiiFigure::cheer(float duration) {
    setState(MiiState::Cheer, duration);
}

void MiiFigure::speak(float duration) {
    setState(MiiState::Speak, duration);
}

void MiiFigure::idle(float duration) {
    setState(MiiState::Idle, duration);
}

nxui::Vec2 MiiFigure::headTopAnchor() const {
    const float effScale = m_scale;
    const float hScale = 0.85f + (m_data.height / 128.0f) * 0.30f;
    const float headR = 19.5f * effScale;
    const float torsoH = 22.0f * hScale * effScale;
    const float footH = 6.0f * effScale;

    float jumpOffset = 0.0f;
    if (m_state == MiiState::Cheer) {
        jumpOffset = std::abs(std::sin(m_animTime * 6.2831853f * 1.5f)) * (13.0f * effScale);
    }
    float torsoY = m_pos.y - footH - torsoH + 1.0f * effScale - jumpOffset;
    float headCenterY = torsoY - headR + 3.0f * effScale;
    float headCenterX = m_pos.x + (m_facingLeft ? -1.0f : 1.0f) * effScale;

    return {headCenterX, headCenterY - headR};
}

void MiiFigure::showSpeechBubble(const SpeechBubbleData& data, float duration) {
    setState(MiiState::Speak, duration);
    m_speechBubble.show(data, headTopAnchor(), duration);
}

void MiiFigure::dismissSpeechBubble(bool immediate) {
    m_speechBubble.dismiss(immediate);
    if (m_state == MiiState::Speak) {
        setState(MiiState::Idle, 0.0f);
    }
}

bool MiiFigure::hitTest(const nxui::Vec2& point) const {
    if (m_speechBubble.isVisible() && m_speechBubble.hitTest(point)) {
        return true;
    }
    float effScale = m_scale;
    float wScale = 0.85f + (m_data.build / 128.0f) * 0.30f;
    float hScale = 0.85f + (m_data.height / 128.0f) * 0.30f;

    float totalHeight = (m_config.baseHeight * hScale) * effScale;
    float totalWidth = (m_config.baseWidth * wScale) * effScale;

    nxui::Rect hitBox{
        m_pos.x - totalWidth * 0.5f,
        m_pos.y - totalHeight,
        totalWidth,
        totalHeight + 8.0f * effScale
    };

    return hitBox.contains(point);
}

void MiiFigure::update(float dt) {
    if (dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f; // Clamp to avoid simulation jump on long frame

    m_animTime += dt;

    if (m_speechBubble.isVisible()) {
        m_speechBubble.setAnchor(headTopAnchor());
        m_speechBubble.update(dt);
        if (!m_speechBubble.isVisible() && m_state == MiiState::Speak) {
            setState(MiiState::Idle, 0.0f);
        }
    }

    if (m_stateTimer > 0.0f) {
        m_stateTimer -= dt;
        if (m_stateTimer <= 0.0f) {
            onStateTimerExpired();
        }
    }

    switch (m_state) {
    case MiiState::Walk: {
        nxui::Vec2 diff = m_targetPos - m_pos;
        float distSq = diff.x * diff.x + diff.y * diff.y;
        if (distSq < 16.0f) { // Reached within 4 pixels
            m_pos = m_targetPos;
            if (m_autonomous) {
                nxui::Vec2 gDiff = m_gatherCenter - m_pos;
                float gDistSq = gDiff.x * gDiff.x + gDiff.y * gDiff.y;
                if (gDistSq <= (m_gatherRadius * 1.5f) * (m_gatherRadius * 1.5f) &&
                    randomFloat(0.0f, 1.0f) < 0.65f) {
                    setState(MiiState::Gather, randomFloat(6.0f, 12.0f));
                    m_facingLeft = (m_gatherCenter.x < m_pos.x);
                } else {
                    setState(MiiState::Idle, randomFloat(2.5f, 5.0f));
                }
            } else {
                setState(MiiState::Idle, 0.0f);
            }
        } else {
            float dist = std::sqrt(distSq);
            nxui::Vec2 dir = diff * (1.0f / dist);
            float step = m_config.walkSpeed * m_scale * dt;
            if (step > dist) step = dist;
            m_pos = m_pos + dir * step;

            if (dir.x < -0.05f) {
                m_facingLeft = true;
            } else if (dir.x > 0.05f) {
                m_facingLeft = false;
            }

            m_walkPhase += dt * (m_config.walkSpeed / 6.0f);
            if (m_walkPhase > 6.2831853f) {
                m_walkPhase -= 6.2831853f;
            }
        }
        break;
    }
    case MiiState::Gather: {
        m_facingLeft = (m_gatherCenter.x < m_pos.x);
        break;
    }
    case MiiState::Idle:
    case MiiState::Cheer:
    case MiiState::Speak:
    default:
        break;
    }
}

void MiiFigure::onStateTimerExpired() {
    if (!m_autonomous) {
        if (m_state != MiiState::Idle) {
            setState(MiiState::Idle, 0.0f);
        }
        return;
    }

    switch (m_state) {
    case MiiState::Idle: {
        float roll = randomFloat(0.0f, 1.0f);
        if (roll < 0.62f) {
            pickNewWanderTarget();
        } else if (roll < 0.85f) {
            setState(MiiState::Cheer, randomFloat(2.0f, 3.2f));
        } else {
            m_facingLeft = !m_facingLeft;
            setState(MiiState::Idle, randomFloat(2.0f, 4.0f));
        }
        break;
    }
    case MiiState::Walk: {
        setState(MiiState::Idle, randomFloat(2.0f, 4.0f));
        break;
    }
    case MiiState::Gather: {
        float roll = randomFloat(0.0f, 1.0f);
        if (roll < 0.40f) {
            setState(MiiState::Cheer, randomFloat(2.2f, 3.5f));
        } else {
            pickNewWanderTarget();
        }
        break;
    }
    case MiiState::Cheer:
    case MiiState::Speak:
    default: {
        setState(MiiState::Idle, randomFloat(2.0f, 4.5f));
        break;
    }
    }
}

void MiiFigure::pickNewWanderTarget() {
    if (m_config.wanderBounds.width <= 0.0f || m_config.wanderBounds.height <= 0.0f) {
        return;
    }

    nxui::Vec2 dest{
        randomFloat(m_config.wanderBounds.x, m_config.wanderBounds.right()),
        randomFloat(m_config.wanderBounds.y, m_config.wanderBounds.bottom())
    };

    walkTo(dest);
}

float MiiFigure::randomFloat(float min, float max) {
    if (min >= max) return min;
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

void MiiFigure::render(nxui::Renderer& ren, nxui::Font* font, nxui::Font* smallFont,
                       float cameraOffsetX, float viewZoom,
                       const nxui::Vec2& viewCenter) const {
    const float effScale = m_scale * viewZoom;
    const nxui::Vec2 renderPos{
        viewCenter.x + (m_pos.x - cameraOffsetX - viewCenter.x) * viewZoom,
        viewCenter.y + (m_pos.y - viewCenter.y) * viewZoom
    };
    // The rendering code below was originally authored around m_pos. Shadow it
    // locally so every primitive, face texture, name pill, and speech-bubble
    // anchor shares the exact same Plaza view transform.
    const nxui::Vec2& m_pos = renderPos;
    const float wScale = 0.85f + (m_data.build / 128.0f) * 0.30f;
    const float hScale = 0.85f + (m_data.height / 128.0f) * 0.30f;

    // Probe at function entry, before any arithmetic, so a bad member is caught
    // as it stands rather than after it has propagated. setScale clamps to a
    // minimum of 0.1 and every caller passes a value near 1.0, so an m_scale
    // outside a generous window means the object's state was damaged after
    // construction rather than configured badly.
    if (ren.probeBudgetLeft()) {
        const bool badState =
            !std::isfinite(m_scale) || m_scale < 0.01f || m_scale > 16.f ||
            !std::isfinite(this->m_pos.x) || !std::isfinite(this->m_pos.y) ||
            std::fabs(this->m_pos.x) > 1e5f || std::fabs(this->m_pos.y) > 1e5f ||
            !std::isfinite(m_animTime) || !std::isfinite(m_walkPhase);
        if (badState) {
            std::uint32_t sb = 0;
            const float sv = m_scale;
            std::memcpy(&sb, &sv, 4);
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "mii.entry state=%d scale=%.6g(0x%08X) pos=(%.6g,%.6g) "
                "target=(%.6g,%.6g) animTime=%.6g walkPhase=%.6g "
                "build=%u height=%u",
                (int)m_state, (double)m_scale, sb,
                (double)this->m_pos.x, (double)this->m_pos.y,
                (double)m_targetPos.x, (double)m_targetPos.y,
                (double)m_animTime, (double)m_walkPhase,
                (unsigned)m_data.build, (unsigned)m_data.height);
            ren.addProbe(buf);
        }
    }

    const float headR = 19.5f * effScale;
    const float torsoW = 20.0f * wScale * effScale;
    const float torsoH = 22.0f * hScale * effScale;
    const float torsoRadius = torsoW * 0.45f;
    const float footW = 11.0f * effScale;
    const float footH = 6.0f * effScale;
    const float handR = 4.5f * effScale;
    const float shadowW = 34.0f * wScale * effScale;
    const float shadowH = 9.0f * effScale;

    // Animation variables
    float jumpOffset = 0.0f;
    float breathBob = 0.0f;
    float walkBounce = 0.0f;
    float leftFootXOffset = 0.0f;
    float leftFootYOffset = 0.0f;
    float rightFootXOffset = 0.0f;
    float rightFootYOffset = 0.0f;

    switch (m_state) {
    case MiiState::Idle:
        breathBob = std::sin(m_animTime * 2.8f) * 1.4f * effScale;
        break;
    case MiiState::Walk:
        walkBounce = std::abs(std::sin(m_walkPhase * 2.0f)) * 2.5f * effScale;
        leftFootXOffset = std::cos(m_walkPhase) * (5.5f * effScale);
        leftFootYOffset = std::max(0.0f, std::sin(m_walkPhase)) * (4.5f * effScale);
        rightFootXOffset = std::cos(m_walkPhase + 3.14159265f) * (5.5f * effScale);
        rightFootYOffset = std::max(0.0f, std::sin(m_walkPhase + 3.14159265f)) * (4.5f * effScale);
        break;
    case MiiState::Gather:
        breathBob = std::sin(m_animTime * 2.4f) * 1.2f * effScale;
        break;
    case MiiState::Cheer:
        jumpOffset = std::abs(std::sin(m_animTime * 6.2831853f * 1.5f)) * (13.0f * effScale);
        leftFootYOffset = jumpOffset * 0.75f;
        rightFootYOffset = jumpOffset * 0.75f;
        break;
    case MiiState::Speak:
        breathBob = std::sin(m_animTime * 3.6f) * 1.8f * effScale;
        break;
    }

    // 1. Floor Drop-Shadow Oval with soft alpha
    float jumpScale = std::min(0.35f, jumpOffset / (40.0f * effScale));
    float currentShadowW = shadowW * (1.0f - jumpScale);
    float currentShadowH = shadowH * (1.0f - jumpScale);
    float shadowAlpha = m_config.shadowOpacity * (1.0f - std::min(0.45f, jumpOffset / (35.0f * effScale)));

    nxui::Rect shadowRect{
        m_pos.x - currentShadowW * 0.5f,
        m_pos.y - currentShadowH * 0.5f + 1.0f * effScale,
        currentShadowW,
        currentShadowH
    };

    // Probe: this draw is the one whose vertices reach addVertex scaled by
    // exactly 2^64. The renderer only sees finished coordinates, so it cannot
    // say which input is wrong. Report the inputs here, at the moment the
    // offending rect is built, and only when the rect is already out of range,
    // so a healthy frame costs one comparison per Mii.
    //
    // Every recorded value divides by 2^64 to a clean constant (0.850, 0.225,
    // 0.275, 0.175, 0.050), and 0.850 is exactly wScale/hScale when build and
    // height are zero. Printing the raw bits of the scale inputs alongside the
    // computed extents distinguishes "a bad input was supplied" from "a correct
    // input was transformed on the way through".
    if (ren.probeBudgetLeft()) {
        const bool outOfRange =
            !(std::isfinite(shadowRect.x) && std::isfinite(shadowRect.y) &&
              std::isfinite(shadowRect.width) && std::isfinite(shadowRect.height)) ||
            std::fabs(shadowRect.x) > 1e5f || std::fabs(shadowRect.y) > 1e5f ||
            std::fabs(shadowRect.width) > 1e5f || std::fabs(shadowRect.height) > 1e5f;
        if (outOfRange) {
            std::uint32_t sb = 0, wb = 0, hb = 0, pb = 0;
            const float scaleV = m_scale, wV = wScale, hV = hScale, posV = m_pos.x;
            std::memcpy(&sb, &scaleV, 4);
            std::memcpy(&wb, &wV, 4);
            std::memcpy(&hb, &hV, 4);
            std::memcpy(&pb, &posV, 4);
            char buf[320];
            std::snprintf(buf, sizeof(buf),
                "mii.shadow state=%d scale=%.6g(0x%08X) build=%u height=%u "
                "wScale=%.6g(0x%08X) hScale=%.6g(0x%08X) "
                "pos=(%.4g[0x%08X],%.4g) shadowWH=(%.6g,%.6g) "
                "jumpOff=%.6g jumpScale=%.6g alpha=%.4g",
                (int)m_state, (double)m_scale, sb,
                (unsigned)m_data.build, (unsigned)m_data.height,
                (double)wScale, wb, (double)hScale, hb,
                (double)m_pos.x, pb, (double)m_pos.y,
                (double)currentShadowW, (double)currentShadowH,
                (double)jumpOffset, (double)jumpScale, (double)shadowAlpha);
            ren.addProbe(buf);
        }
    }

    ren.drawRoundedRect(shadowRect, nxui::Color(0.0f, 0.0f, 0.0f, shadowAlpha), currentShadowH * 0.5f);

    // 2. Feet (Shoes)
    nxui::Color shoeColor(0.12f, 0.12f, 0.14f);
    nxui::Rect footRectLeft{
        m_pos.x - 7.0f * effScale + leftFootXOffset - footW * 0.5f,
        m_pos.y - footH - leftFootYOffset,
        footW,
        footH
    };
    nxui::Rect footRectRight{
        m_pos.x + 7.0f * effScale + rightFootXOffset - footW * 0.5f,
        m_pos.y - footH - rightFootYOffset,
        footW,
        footH
    };
    ren.drawRoundedRect(footRectLeft, shoeColor, footH * 0.5f);
    ren.drawRoundedRect(footRectRight, shoeColor, footH * 0.5f);

    // 3. Torso and Pants
    float torsoY = m_pos.y - footH - torsoH + 1.0f * effScale - jumpOffset + walkBounce - breathBob;
    float torsoX = m_pos.x - torsoW * 0.5f;

    // Pants connector
    nxui::Color pantsColor = m_data.isUserAccount ? nxui::Color(0.15f, 0.16f, 0.22f) : nxui::Color(0.18f, 0.18f, 0.20f);
    float pantsW = 16.0f * wScale * effScale;
    float pantsH = 7.0f * effScale;
    nxui::Rect pantsRect{m_pos.x - pantsW * 0.5f, torsoY + torsoH - 3.0f * effScale, pantsW, pantsH};
    ren.drawRoundedRect(pantsRect, pantsColor, 2.5f * effScale);

    // Torso Capsule (Favorite shirt color)
    nxui::Color shirtColor = getFavoriteColorRgb(m_data.shirtColor);
    nxui::Rect torsoRect{torsoX, torsoY, torsoW, torsoH};
    ren.drawRoundedRect(torsoRect, shirtColor, torsoRadius);

    // White shirt collar accent
    nxui::Rect collarRect{m_pos.x - 3.2f * effScale, torsoY, 6.4f * effScale, 3.2f * effScale};
    ren.drawRoundedRect(collarRect, nxui::Color(1.0f, 1.0f, 1.0f, 0.65f), 1.6f * effScale);

    // Subtle edge highlight on torso
    ren.drawRoundedRectOutline(torsoRect, nxui::Color(1.0f, 1.0f, 1.0f, 0.20f), torsoRadius, 1.0f);

    // 4. Head and Facial Avatar
    float headCenterY = torsoY - headR + 3.0f * effScale;
    float headCenterX = m_pos.x + (m_facingLeft ? -1.0f : 1.0f) * effScale;
    nxui::Rect headRect{headCenterX - headR, headCenterY - headR, headR * 2.0f, headR * 2.0f};

    nxui::Color skinTone(0.96f, 0.82f, 0.71f);
    if (m_data.headTexture) {
        ren.drawTextureRounded(m_data.headTexture.get(), headRect, headR, nxui::Color::white());
    } else {
        // High quality procedural face fallback
        ren.drawCircle(nxui::Vec2{headCenterX, headCenterY}, headR, skinTone);
        ren.drawCircle(nxui::Vec2{headCenterX - 6.0f * effScale, headCenterY - 2.0f * effScale}, 2.0f * effScale, nxui::Color(0.15f, 0.15f, 0.15f));
        ren.drawCircle(nxui::Vec2{headCenterX + 6.0f * effScale, headCenterY - 2.0f * effScale}, 2.0f * effScale, nxui::Color(0.15f, 0.15f, 0.15f));
        nxui::Rect mouthRect{headCenterX - 4.5f * effScale, headCenterY + 3.5f * effScale, 9.0f * effScale, 3.5f * effScale};
        ren.drawRoundedRect(mouthRect, nxui::Color(0.15f, 0.15f, 0.15f), 1.7f * effScale);
    }
    // Crisp antialiased head rim
    ren.drawRoundedRectOutline(headRect, nxui::Color(0.1f, 0.1f, 0.1f, 0.26f), headR, 1.2f);

    // 5. Hands (Spherical animated hands)
    nxui::Vec2 handLeft;
    nxui::Vec2 handRight;

    switch (m_state) {
    case MiiState::Walk:
        handLeft = {
            torsoX - handR * 0.8f + std::sin(m_walkPhase + 3.14159265f) * (4.5f * effScale),
            torsoY + torsoH * 0.55f + std::cos(m_walkPhase + 3.14159265f) * (2.0f * effScale)
        };
        handRight = {
            torsoX + torsoW + handR * 0.8f + std::sin(m_walkPhase) * (4.5f * effScale),
            torsoY + torsoH * 0.55f + std::cos(m_walkPhase) * (2.0f * effScale)
        };
        break;
    case MiiState::Cheer:
        handLeft = {
            headCenterX - headR - 3.5f * effScale,
            headCenterY - headR * 0.35f + std::sin(m_animTime * 9.0f) * (3.5f * effScale)
        };
        handRight = {
            headCenterX + headR + 3.5f * effScale,
            headCenterY - headR * 0.35f + std::cos(m_animTime * 9.0f) * (3.5f * effScale)
        };
        break;
    case MiiState::Speak:
        handLeft = {
            torsoX - handR * 0.8f,
            torsoY + torsoH * 0.38f + std::sin(m_animTime * 4.5f) * (3.0f * effScale)
        };
        handRight = {
            torsoX + torsoW + handR * 0.8f,
            torsoY + torsoH * 0.60f
        };
        break;
    case MiiState::Idle:
    case MiiState::Gather:
    default:
        handLeft = {
            torsoX - handR * 0.8f,
            torsoY + torsoH * 0.55f + std::sin(m_animTime * 2.8f) * (1.0f * effScale)
        };
        handRight = {
            torsoX + torsoW + handR * 0.8f,
            torsoY + torsoH * 0.55f + std::sin(m_animTime * 2.8f) * (1.0f * effScale)
        };
        break;
    }

    ren.drawCircle(handLeft, handR, skinTone);
    ren.drawCircle(handRight, handR, skinTone);

    // 6. Name Pill
    if (m_config.showNamePill && !m_data.nickname.empty()) {
        nxui::Vec2 textSize = font ? font->measure(m_data.nickname) : nxui::Vec2{36.0f, 12.0f};
        float fontScale = 0.56f * effScale;
        float pillW = textSize.x * fontScale + 14.0f * effScale;
        float pillH = 16.0f * effScale;
        float pillR = pillH * 0.5f;

        float pillY = m_config.namePillAbove
            ? (headRect.y - pillH - 4.0f * effScale)
            : (m_pos.y + 6.0f * effScale);
        float pillX = m_pos.x - pillW * 0.5f;
        nxui::Rect pillRect{pillX, pillY, pillW, pillH};

        nxui::Color pillBg(0.06f, 0.07f, 0.09f, 0.70f);
        ren.drawRoundedRect(pillRect, pillBg, pillR);

        if (m_data.isUserAccount) {
            // VIP gold border for real console profile accounts
            ren.drawRoundedRectOutline(pillRect, nxui::Color(1.0f, 0.84f, 0.25f, 0.85f), pillR, 1.2f);
        } else {
            ren.drawRoundedRectOutline(pillRect, nxui::Color(1.0f, 1.0f, 1.0f, 0.22f), pillR, 1.0f);
        }

        if (font) {
            nxui::Vec2 textPos{
                pillX + (pillW - textSize.x * fontScale) * 0.5f,
                pillY + (pillH - textSize.y * fontScale) * 0.5f - 1.0f * effScale
            };
            ren.drawText(m_data.nickname, textPos, font, nxui::Color(0.96f, 0.96f, 0.98f), fontScale);
        }
    }

    // 7. Speech Bubble
    if (m_speechBubble.isVisible()) {
        m_speechBubble.render(ren, font, smallFont);
    }
}

} // namespace warawara
