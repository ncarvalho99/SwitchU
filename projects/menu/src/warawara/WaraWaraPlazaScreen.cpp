#include "WaraWaraPlazaScreen.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace warawara {

WaraWaraPlazaScreen::WaraWaraPlazaScreen() {
    setRect({0.0f, 0.0f, 1280.0f, 720.0f});
    setVisible(false);
    setFocusable(true);
    setTag("warawara_plaza_screen");

    std::random_device rd;
    m_rng.seed(rd());
}

void WaraWaraPlazaScreen::setupCommunities(const std::vector<GameCommunityEntry>& entries) {
    struct FallbackCommunity {
        uint64_t titleId;
        const char* title;
        const char* subtitle;
    };

    static const FallbackCommunity kFallbacks[] = {
        {0x0100000000001000ULL, "SwitchU Community", "Latest News & Tips"},
        {0x0100000000001001ULL, "Miiverse Plaza", "Drawings & Reactions"},
        {0x0100000000001002ULL, "Nintendo eShop", "Featured Titles"},
        {0x0100000000001003ULL, "Mii Maker", "Create & Customize"},
        {0x0100000000001004ULL, "Activity Log", "Play Records & Stats"},
        {0x0100000000001005ULL, "Theme Shop", "Wii U Style Themes"},
        {0x0100000000001006ULL, "Super Mario", "Mushroom Kingdom Hub"},
        {0x0100000000001007ULL, "The Legend of Zelda", "Hyrule Gathering"},
        {0x0100000000001008ULL, "Mario Kart", "Grand Prix Community"},
        {0x0100000000001009ULL, "Super Smash Bros.", "Fighter Battle Arena"}
    };

    std::vector<PlazaCommunityData> communityList;
    communityList.reserve(10);

    // 1. Add installed game communities
    for (const auto& entry : entries) {
        if (communityList.size() >= 10) break;
        PlazaCommunityData data;
        data.titleId = entry.titleId;
        data.titleName = entry.title;
        data.subtitle = entry.subtitle.empty() ? "Community Gathering" : entry.subtitle;
        data.iconTexture = entry.iconTexture;
        communityList.push_back(data);
    }

    // 2. Gracefully pad up to 10 communities using authentic Nintendo Wii U fallbacks
    size_t fallbackIdx = 0;
    while (communityList.size() < 10 && fallbackIdx < 10) {
        const auto& fb = kFallbacks[fallbackIdx++];
        bool alreadyExists = false;
        for (const auto& c : communityList) {
            if (c.titleName == fb.title) {
                alreadyExists = true;
                break;
            }
        }
        if (!alreadyExists) {
            PlazaCommunityData data;
            data.titleId = fb.titleId;
            data.titleName = fb.title;
            data.subtitle = fb.subtitle;
            data.iconTexture = nullptr;
            communityList.push_back(data);
        }
    }

    // If pedestals are already built with matching communities, update data (textures/playtime)
    // without reallocating pedestals or rebuilding Miis for instantaneous opening
    if (m_pedestals.size() == communityList.size() && !m_miis.empty()) {
        bool matches = true;
        for (size_t i = 0; i < communityList.size(); ++i) {
            if (m_pedestals[i]->data().titleId != communityList[i].titleId) {
                matches = false;
                break;
            }
        }
        if (matches) {
            for (size_t i = 0; i < communityList.size(); ++i) {
                m_pedestals[i]->setData(communityList[i]);
            }
            return;
        }
    }

    m_pedestals.clear();

    // 3. Position the 10 pedestals across the 2100px wide virtual plaza in two perspective depth tiers
    // Back tier (5 pedestals): Y = 320.0f, scale = 0.88f
    const float backX[] = {230.0f, 640.0f, 1050.0f, 1460.0f, 1870.0f};
    // Front tier (5 pedestals): Y = 510.0f, scale = 1.06f
    const float frontX[] = {150.0f, 560.0f, 970.0f, 1380.0f, 1790.0f};

    for (size_t i = 0; i < communityList.size() && i < 10; ++i) {
        if (i < 5) {
            // Back tier
            auto ped = std::make_unique<PlazaPedestal>(
                communityList[i],
                nxui::Vec2{backX[i], 320.0f},
                72.0f,
                0.88f
            );
            m_pedestals.push_back(std::move(ped));
        } else {
            // Front tier
            size_t fi = i - 5;
            auto ped = std::make_unique<PlazaPedestal>(
                communityList[i],
                nxui::Vec2{frontX[fi], 510.0f},
                82.0f,
                1.06f
            );
            m_pedestals.push_back(std::move(ped));
        }
    }

    populateMiis();
}

void WaraWaraPlazaScreen::populateMiis() {
    m_miis.clear();

    if (!m_avatarManager) {
        return;
    }

    const auto& allAvatars = m_avatarManager->avatars();
    if (allAvatars.empty()) {
        return;
    }

    // Target ~35 Miis total across the plaza
    const size_t targetMiiCount = std::min<size_t>(36, std::max<size_t>(20, allAvatars.size() * 2));
    m_miis.reserve(targetMiiCount);

    size_t avatarIdx = 0;

    // 1. Assign 3 Miis to gather around each pedestal
    for (size_t p = 0; p < m_pedestals.size(); ++p) {
        auto& ped = m_pedestals[p];
        for (size_t s = 0; s < 3; ++s) {
            const auto& av = allAvatars[avatarIdx % allAvatars.size()];
            avatarIdx++;

            auto mii = std::make_unique<MiiFigure>(av);
            nxui::Vec2 slotPos = ped->getGatheringSlot(s);

            // Stagger scale slightly for visual variety
            float baseScale = (ped->scale() < 1.0f) ? 0.88f : 1.02f;
            mii->setScale(baseScale + ((s % 3) - 1) * 0.05f);

            // Place near slot and command them to gather
            mii->setPosition({slotPos.x + ((s % 2 == 0) ? -12.0f : 12.0f), slotPos.y + 6.0f});
            mii->gatherAt(slotPos, 14.0f);
            mii->setAutonomous(true);

            m_miis.push_back(std::move(mii));
        }
    }

    // 2. Add 5-6 roaming Miis that stroll freely across the plaza
    for (size_t r = 0; r < 6; ++r) {
        const auto& av = allAvatars[avatarIdx % allAvatars.size()];
        avatarIdx++;

        auto mii = std::make_unique<MiiFigure>(av);
        float rx = 100.0f + r * 340.0f;
        float ry = (r % 2 == 0) ? 360.0f : 550.0f;
        mii->setPosition({rx, ry});
        mii->setScale(0.96f);
        mii->setWanderBounds(nxui::Rect{60.0f, 290.0f, m_plazaWidth - 120.0f, 330.0f});
        mii->setAutonomous(true);
        mii->idle(1.5f + r * 0.8f);

        m_miis.push_back(std::move(mii));
    }
}

void WaraWaraPlazaScreen::open() {
    m_active = true;
    setVisible(true);
    m_cameraTargetX = (m_plazaWidth - 1280.0f) * 0.5f; // Center camera on opening
    m_cameraX = m_cameraTargetX;
    m_focusedPedestalIndex = -1;
    m_focusedMii = nullptr;
    m_speechBubbleTimer = 2.0f;

    // Reset initial cheers for welcoming feel
    for (size_t i = 0; i < m_miis.size(); ++i) {
        if (i % 5 == 0) {
            m_miis[i]->cheer(2.2f);
        }
    }
}

void WaraWaraPlazaScreen::close() {
    m_active = false;
    for (auto& mii : m_miis) {
        mii->dismissSpeechBubble(true);
    }
    if (m_animalesePlayer) {
        m_animalesePlayer->stop();
    }
}

void WaraWaraPlazaScreen::triggerRandomSpeechBubble() {
    if (!m_dialogueEngine || m_miis.empty()) return;

    // Find candidate Miis visible on screen that don't already have an active bubble
    std::vector<MiiFigure*> candidates;
    for (auto& mii : m_miis) {
        float sx = mii->position().x - m_cameraX;
        if (sx >= 60.0f && sx <= 1220.0f && !mii->hasSpeechBubble()) {
            candidates.push_back(mii.get());
        }
    }

    if (candidates.empty()) return;

    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    MiiFigure* chosenMii = candidates[dist(m_rng)];

    // Check if chosen Mii is near a community pedestal
    const PlazaPedestal* nearbyPed = nullptr;
    for (const auto& ped : m_pedestals) {
        float dx = ped->position().x - chosenMii->position().x;
        float dy = ped->position().y - chosenMii->position().y;
        if (std::sqrt(dx * dx + dy * dy) < 140.0f) {
            nearbyPed = ped.get();
            break;
        }
    }

    SpeechBubbleData bubbleData;
    if (nearbyPed && nearbyPed->data().titleId != 0) {
        bubbleData = m_dialogueEngine->getGameTipDialogue(
            nearbyPed->data().titleId,
            nearbyPed->data().titleName,
            chosenMii->data().nickname
        );
    } else {
        bubbleData = m_dialogueEngine->getDialogueForMii(
            chosenMii->data(),
            {},
            m_activityLog
        );
    }

    chosenMii->showSpeechBubble(bubbleData, 6.0f);

    if (m_animalesePlayer) {
        m_animalesePlayer->speak(bubbleData.text, chosenMii->data().gender, 1.0f);
    }
}

void WaraWaraPlazaScreen::interactWithMii(MiiFigure* mii) {
    if (!mii) return;

    mii->cheer(3.0f);

    SpeechBubbleData bubble;
    if (m_dialogueEngine) {
        bubble = m_dialogueEngine->getDialogueForMii(mii->data(), {}, m_activityLog);
    } else {
        bubble.text = "Hello! Having fun in WaraWara Plaza?";
        bubble.author = mii->data().nickname;
        bubble.yeahCount = 12;
    }

    mii->showSpeechBubble(bubble, 6.5f);

    if (m_animalesePlayer) {
        m_animalesePlayer->speak(bubble.text, mii->data().gender, 1.05f);
    }
}

void WaraWaraPlazaScreen::update(float dt) {
    m_time += dt;

    // Fade transition
    const float fadeSpeed = 5.0f;
    if (m_active) {
        m_fadeAlpha = std::min(1.0f, m_fadeAlpha + dt * fadeSpeed);
    } else {
        m_fadeAlpha = std::max(0.0f, m_fadeAlpha - dt * fadeSpeed);
        if (m_fadeAlpha <= 0.001f) {
            setVisible(false);
            return;
        }
    }

    // Camera smoothing
    const float maxCameraX = std::max(0.0f, m_plazaWidth - 1280.0f);
    m_cameraTargetX = std::clamp(m_cameraTargetX, 0.0f, maxCameraX);
    m_cameraX += (m_cameraTargetX - m_cameraX) * std::min(1.0f, dt * 7.5f);

    // Update pedestals
    for (auto& ped : m_pedestals) {
        ped->update(dt);
    }

    // Update Miis
    for (auto& mii : m_miis) {
        mii->update(dt);
    }

    // Periodic speech bubble chatter
    m_speechBubbleTimer -= dt;
    if (m_speechBubbleTimer <= 0.0f) {
        m_speechBubbleTimer = m_speechBubbleInterval;
        triggerRandomSpeechBubble();
    }
}

bool WaraWaraPlazaScreen::handleInput(const nxui::Input& input, float dt) {
    if (!m_active || m_fadeAlpha < 0.2f) return false;

    // 1. Controller Return: B button
    if (input.isDown(nxui::Button::B)) {
        close();
        if (m_closeCb) m_closeCb();
        return true;
    }

    // 2. Camera Panning via Left Stick or Right Stick
    const float panSpeed = 740.0f;
    float lx = input.leftStickX();
    if (std::abs(lx) > 0.15f) {
        m_cameraTargetX += lx * panSpeed * dt;
        if (m_focusedPedestalIndex >= 0) {
            m_pedestals[m_focusedPedestalIndex]->setFocused(false);
            m_focusedPedestalIndex = -1;
        }
    }

    float rx = input.rightStickX();
    if (std::abs(rx) > 0.15f) {
        m_cameraTargetX += rx * panSpeed * dt;
        if (m_focusedPedestalIndex >= 0) {
            m_pedestals[m_focusedPedestalIndex]->setFocused(false);
            m_focusedPedestalIndex = -1;
        }
    }

    // 3. Camera Panning via D-Pad
    if (input.isHeld(nxui::Button::DLeft)) {
        m_cameraTargetX -= panSpeed * 0.9f * dt;
        if (m_focusedPedestalIndex >= 0) {
            m_pedestals[m_focusedPedestalIndex]->setFocused(false);
            m_focusedPedestalIndex = -1;
        }
    }
    if (input.isHeld(nxui::Button::DRight)) {
        m_cameraTargetX += panSpeed * 0.9f * dt;
        if (m_focusedPedestalIndex >= 0) {
            m_pedestals[m_focusedPedestalIndex]->setFocused(false);
            m_focusedPedestalIndex = -1;
        }
    }

    // Clamp camera within virtual plaza bounds
    const float maxCamX = std::max(0.0f, m_plazaWidth - 1280.0f);
    m_cameraTargetX = std::clamp(m_cameraTargetX, 0.0f, maxCamX);

    // 4. Talk with Miis or Launch Game via A button, X button, or Y button
    if (input.isDown(nxui::Button::A) || input.isDown(nxui::Button::X) || input.isDown(nxui::Button::Y)) {
        if (input.isDown(nxui::Button::A) && m_focusedPedestalIndex >= 0 && m_focusedPedestalIndex < (int)m_pedestals.size()) {
            uint64_t tid = m_pedestals[m_focusedPedestalIndex]->data().titleId;
            if (m_launchGameCb && tid != 0) {
                m_launchGameCb(tid);
                return true;
            }
        }

        // Find the Mii closest to the screen center
        float viewCenterX = m_cameraX + 640.0f;
        MiiFigure* bestMii = nullptr;
        float bestDist = 1e9f;

        for (auto& mii : m_miis) {
            float sx = mii->position().x - m_cameraX;
            if (sx >= 40.0f && sx <= 1240.0f) {
                float dx = mii->position().x - viewCenterX;
                float dy = mii->position().y - 480.0f;
                float dist = dx * dx + dy * dy;
                if (mii->hasSpeechBubble()) {
                    dist += 400.0f * 400.0f;
                }
                if (dist < bestDist) {
                    bestDist = dist;
                    bestMii = mii.get();
                }
            }
        }

        if (bestMii) {
            interactWithMii(bestMii);
            return true;
        } else {
            triggerRandomSpeechBubble();
            return true;
        }
    }

    return true;
}

bool WaraWaraPlazaScreen::handleTouch(const nxui::Input& input) {
    if (!m_active || m_fadeAlpha < 0.2f) return false;

    float tx = input.touchX();
    float ty = input.touchY();

    if (input.touchDown()) {
        // Check top right close area
        if (tx >= 1180.0f && ty <= 70.0f) {
            close();
            if (m_closeCb) m_closeCb();
            return true;
        }

        // Check if tapping a Mii
        nxui::Vec2 worldTouch{tx + m_cameraX, ty};
        for (auto it = m_miis.rbegin(); it != m_miis.rend(); ++it) {
            if ((*it)->hitTest(worldTouch)) {
                interactWithMii(it->get());
                return true;
            }
        }

        // Check if tapping a Pedestal
        for (size_t i = 0; i < m_pedestals.size(); ++i) {
            if (m_pedestals[i]->hitTest({tx, ty}, m_cameraX)) {
                m_focusedPedestalIndex = (int)i;
                for (size_t j = 0; j < m_pedestals.size(); ++j) {
                    m_pedestals[j]->setFocused(j == i);
                }
                uint64_t tid = m_pedestals[i]->data().titleId;
                if (m_launchGameCb && tid != 0) {
                    m_launchGameCb(tid);
                }
                return true;
            }
        }

        m_lastTouchX = tx;
        m_isDraggingTouch = true;
        return true;
    }

    if (input.isTouching() && m_isDraggingTouch) {
        float dx = m_lastTouchX - tx;
        m_cameraTargetX += dx * 1.15f;
        m_lastTouchX = tx;
        return true;
    }

    if (input.touchUp()) {
        m_isDraggingTouch = false;
        return true;
    }

    return false;
}

void WaraWaraPlazaScreen::drawPlazaFloor(nxui::Renderer& ren) const {
    // 1. Authentic Wii U Clean Sky / Horizon Gradient
    nxui::Rect skyRect{0.0f, 0.0f, 1280.0f, 210.0f};
    ren.drawGradientRect(skyRect, nxui::Color(0.89f, 0.93f, 0.97f, m_fadeAlpha),
                                  nxui::Color(0.80f, 0.86f, 0.93f, m_fadeAlpha));

    // 2. Horizon divider line
    ren.drawLine({0.0f, 210.0f}, {1280.0f, 210.0f}, nxui::Color(0.74f, 0.80f, 0.88f, 0.65f * m_fadeAlpha), 1.5f);

    // 3. Ground Floor Base
    nxui::Rect groundRect{0.0f, 210.0f, 1280.0f, 510.0f};
    ren.drawGradientRect(groundRect, nxui::Color(0.86f, 0.90f, 0.95f, m_fadeAlpha),
                                     nxui::Color(0.76f, 0.82f, 0.89f, m_fadeAlpha));

    // 4. Concentric Circular Plaza Floor Arcs & Radial Tiles (Wii U plaza disc geometry)
    const float plazaCenterX = (m_plazaWidth * 0.5f) - m_cameraX;
    const float floorCenterY = 820.0f; // Virtual center below screen for gentle upward curving arcs

    const float ringRadii[] = {340.0f, 480.0f, 620.0f, 760.0f, 900.0f};
    for (float r : ringRadii) {
        // Draw wide elliptical floor disc arc
        nxui::Rect ringRect{plazaCenterX - r * 1.55f, floorCenterY - r * 0.72f, r * 3.10f, r * 1.44f};
        ren.drawRoundedRectOutline(ringRect, nxui::Color(1.0f, 1.0f, 1.0f, 0.28f * m_fadeAlpha), ringRect.height * 0.5f, 1.6f);
    }

    // Radial perspective lines
    const float angles[] = {-0.65f, -0.45f, -0.25f, -0.08f, 0.08f, 0.25f, 0.45f, 0.65f};
    for (float a : angles) {
        float x1 = plazaCenterX + std::sin(a) * 320.0f;
        float y1 = 210.0f;
        float x2 = plazaCenterX + std::sin(a * 1.6f) * 980.0f;
        float y2 = 720.0f;
        ren.drawLine({x1, y1}, {x2, y2}, nxui::Color(1.0f, 1.0f, 1.0f, 0.20f * m_fadeAlpha), 1.2f);
    }
}

void WaraWaraPlazaScreen::drawHeader(nxui::Renderer& ren) const {
    // 1. "WaraWara Plaza" Title Pill in top center
    const float titleW = 260.0f;
    const float titleH = 44.0f;
    const float titleX = (1280.0f - titleW) * 0.5f;
    const float titleY = 18.0f;
    nxui::Rect titleRect{titleX, titleY, titleW, titleH};

    // Frosted glass background
    ren.drawRoundedRect(titleRect, nxui::Color(0.08f, 0.14f, 0.24f, 0.62f * m_fadeAlpha), titleH * 0.5f);
    ren.drawRoundedRectOutline(titleRect, nxui::Color(0.35f, 0.75f, 1.0f, 0.70f * m_fadeAlpha), titleH * 0.5f, 1.5f);

    if (m_fontNormal) {
        std::string titleText = "WaraWara Plaza";
        nxui::Vec2 tSize = m_fontNormal->measure(titleText);
        float tScale = 0.68f;
        nxui::Vec2 tPos{
            titleX + (titleW - tSize.x * tScale) * 0.5f,
            titleY + (titleH - tSize.y * tScale) * 0.5f - 1.0f
        };
        ren.drawText(titleText, tPos, m_fontNormal, nxui::Color(0.97f, 0.98f, 1.0f, m_fadeAlpha), tScale);
    }
}

void WaraWaraPlazaScreen::render(nxui::Renderer& ren) {
    if (m_fadeAlpha <= 0.001f) return;

    // 1. Draw Plaza Sky & Floor
    drawPlazaFloor(ren);

    // 2. Render Depth-Sorted Scene
    // Depth sorting strategy for authentic perspective:
    // a. Back row pedestals (Y = 320.0f)
    for (size_t i = 0; i < m_pedestals.size() && i < 5; ++i) {
        m_pedestals[i]->render(ren, m_fontNormal, m_fontSmall, m_cameraX);
    }

    // b. Miis located in the upper depth tier (Y < 420.0f)
    for (const auto& mii : m_miis) {
        if (mii->position().y < 420.0f) {
            // Temporarily apply camera offset for rendering
            nxui::Vec2 origPos = mii->position();
            mii->setPosition({origPos.x - m_cameraX, origPos.y});
            mii->render(ren, m_fontNormal, m_fontSmall);
            mii->setPosition(origPos);
        }
    }

    // c. Front row pedestals (Y = 510.0f)
    for (size_t i = 5; i < m_pedestals.size(); ++i) {
        m_pedestals[i]->render(ren, m_fontNormal, m_fontSmall, m_cameraX);
    }

    // d. Miis located in the lower depth tier (Y >= 420.0f)
    for (const auto& mii : m_miis) {
        if (mii->position().y >= 420.0f) {
            nxui::Vec2 origPos = mii->position();
            mii->setPosition({origPos.x - m_cameraX, origPos.y});
            mii->render(ren, m_fontNormal, m_fontSmall);
            mii->setPosition(origPos);
        }
    }

    // 3. Header & Navigation UI
    drawHeader(ren);
}

} // namespace warawara
