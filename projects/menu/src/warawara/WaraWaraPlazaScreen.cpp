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

    // Application::dispatchInput() sends actions to the active FocusManager
    // before WiiUMenuApp::onUpdate() runs. Binding A here is therefore the
    // reliable controller path: once openWaraWaraPlaza() explicitly focuses
    // this screen, the press is consumed here and cannot depend on a parallel
    // manual poll running later in the frame.
    addAction(static_cast<std::uint64_t>(nxui::Button::A), [this]() {
        if (m_active)
            activateHandTarget();
    });

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
            // Fallback communities are presentation-only; zero prevents the
            // hand's A action from pretending they are installed titles.
            data.titleId = 0;
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

    // Arrange installed-title communities around one elliptical gathering
    // plaza, matching the Wii U's radial overview instead of a rigid two-row
    // shelf. The rear arc is smaller and the front arc larger, establishing
    // depth while keeping all ten titles visible at the default zoom.
    const nxui::Vec2 kPlazaCenter{640.0f, 430.0f};
    static constexpr float kRadiusX = 475.0f;
    static constexpr float kRadiusY = 185.0f;
    static constexpr float kPi = 3.14159265358979323846f;
    const size_t count = std::min<size_t>(communityList.size(), 10);

    for (size_t i = 0; i < count; ++i) {
        // Start at the top and proceed clockwise. A half-slot offset for even
        // counts avoids stacking a title directly behind the centre Miis.
        const float phase = -kPi * 0.5f +
            (2.0f * kPi * (static_cast<float>(i) + 0.5f)) / static_cast<float>(count);
        const float depth = 0.5f + 0.5f * std::sin(phase);
        const nxui::Vec2 pos{
            kPlazaCenter.x + std::cos(phase) * kRadiusX,
            kPlazaCenter.y + std::sin(phase) * kRadiusY
        };
        const float perspectiveScale = 0.76f + depth * 0.30f;
        m_pedestals.push_back(std::make_unique<PlazaPedestal>(
            communityList[i], pos, 68.0f, perspectiveScale));
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

            // Stagger scale slightly for visual variety.
            // `s` is size_t, so `(s % 3) - 1` is evaluated in unsigned
            // arithmetic and wraps to SIZE_MAX when s == 0. Converting that to
            // float yields exactly 2^64, which setScale's lower-bound-only
            // clamp accepts. Compute the stagger in signed arithmetic.
            float baseScale = (ped->scale() < 1.0f) ? 0.88f : 1.02f;
            const int stagger = static_cast<int>(s % 3) - 1;
            mii->setScale(baseScale + static_cast<float>(stagger) * 0.05f);

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
    m_cameraTargetX = 0.0f;
    m_cameraX = 0.0f;
    // Begin close on the welcoming Miis, then ease out to the full radial
    // overview like the Wii U's opening presentation.
    m_zoomTarget = 1.0f;
    m_zoom = 1.28f;
    m_focusedPedestalIndex = -1;
    m_focusedMii = nullptr;
    m_speechBubbleTimer = 2.0f;

    if (!m_pedestals.empty()) {
        // Select the front-centre community on entry, where a viewer naturally
        // looks after the reference intro pulls back to the overview.
        int front = 0;
        for (size_t i = 1; i < m_pedestals.size(); ++i) {
            if (m_pedestals[i]->position().y > m_pedestals[front]->position().y)
                front = static_cast<int>(i);
        }
        moveHandToPedestal(front, false);
    }

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
        const float sx = worldToScreen(mii->position()).x;
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

MiiFigure* WaraWaraPlazaScreen::nearestVisibleMii() const {
    const float viewCenterX = m_cameraX + 640.0f;
    MiiFigure* bestMii = nullptr;
    float bestDist = 1e9f;

    for (const auto& mii : m_miis) {
        const float screenX = mii->position().x - m_cameraX;
        if (screenX < 40.0f || screenX > 1240.0f)
            continue;

        const float dx = mii->position().x - viewCenterX;
        const float dy = mii->position().y - 480.0f;
        float dist = dx * dx + dy * dy;
        // Prefer a Mii without an existing bubble without making an already
        // speaking Mii impossible to select when it is the only visible one.
        if (mii->hasSpeechBubble())
            dist += 400.0f * 400.0f;
        if (dist < bestDist) {
            bestDist = dist;
            bestMii = mii.get();
        }
    }
    return bestMii;
}

bool WaraWaraPlazaScreen::interactWithNearestVisibleMii() {
    if (auto* mii = nearestVisibleMii()) {
        interactWithMii(mii);
        return true;
    }
    triggerRandomSpeechBubble();
    return !m_miis.empty();
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

    // Smooth zoom gives the Wii U-style pull-in/pull-out without a hard scene
    // cut. The radial layout fits the viewport, so horizontal camera offset is
    // retained only for compatibility with the existing render helpers.
    m_zoomTarget = std::clamp(m_zoomTarget, 0.78f, 1.28f);
    m_zoom += (m_zoomTarget - m_zoom) * std::min(1.0f, dt * 8.0f);
    m_cameraTargetX = 0.0f;
    m_cameraX += (m_cameraTargetX - m_cameraX) * std::min(1.0f, dt * 7.5f);
    m_handPulse += dt;

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

    if (input.isDown(nxui::Button::B)) {
        close();
        if (m_closeCb) m_closeCb();
        return true;
    }

    // The left stick is an analogue Wii Remote-style hand pointer. D-pad
    // navigation snaps that same hand between communities for controllers
    // where precise pointer motion is less comfortable.
    const float lx = input.leftStickX();
    const float ly = input.leftStickY();
    const float stickMagnitude = std::sqrt(lx * lx + ly * ly);
    if (stickMagnitude > 0.16f) {
        const float pointerSpeed = 620.0f;
        m_handPos.x += lx * pointerSpeed * dt;
        m_handPos.y -= ly * pointerSpeed * dt;
        m_handPos.x = std::clamp(m_handPos.x, 26.0f, 1254.0f);
        m_handPos.y = std::clamp(m_handPos.y, 92.0f, 684.0f);
        updateHandSelection();
    }

    nxui::Vec2 navDir{0.0f, 0.0f};
    if (input.isDown(nxui::Button::DLeft)) navDir.x = -1.0f;
    else if (input.isDown(nxui::Button::DRight)) navDir.x = 1.0f;
    else if (input.isDown(nxui::Button::DUp)) navDir.y = -1.0f;
    else if (input.isDown(nxui::Button::DDown)) navDir.y = 1.0f;

    if (navDir.x != 0.0f || navDir.y != 0.0f) {
        int next = findDirectionalPedestal(m_focusedPedestalIndex, navDir);
        if (next >= 0) moveHandToPedestal(next, true);
    }

    // Triggers provide stepped zoom; right-stick vertical movement provides
    // smooth zoom. Up/pull-back zooms in, down/push-forward zooms out.
    if (input.isDown(nxui::Button::ZR)) m_zoomTarget += 0.10f;
    if (input.isDown(nxui::Button::ZL)) m_zoomTarget -= 0.10f;
    const float ry = input.rightStickY();
    if (std::abs(ry) > 0.18f) m_zoomTarget += ry * 0.55f * dt;
    m_zoomTarget = std::clamp(m_zoomTarget, 0.78f, 1.28f);

    // X/Y preserve the existing direct Mii talk shortcut. A is dispatched once
    // through the focused widget's registered action and activates the hand's
    // selected game; keeping it out of this poll avoids duplicate activation.
    if (input.isDown(nxui::Button::X) || input.isDown(nxui::Button::Y)) {
        interactWithNearestVisibleMii();
        return true;
    }

    return true;
}

nxui::Vec2 WaraWaraPlazaScreen::worldToScreen(const nxui::Vec2& world) const {
    const nxui::Vec2 kViewCenter{640.0f, 400.0f};
    return {
        kViewCenter.x + (world.x - m_cameraX - kViewCenter.x) * m_zoom,
        kViewCenter.y + (world.y - kViewCenter.y) * m_zoom
    };
}

void WaraWaraPlazaScreen::moveHandToPedestal(int index, bool playSfx) {
    if (index < 0 || index >= static_cast<int>(m_pedestals.size())) return;

    const bool changed = m_focusedPedestalIndex != index;
    m_focusedPedestalIndex = index;
    for (size_t i = 0; i < m_pedestals.size(); ++i)
        m_pedestals[i]->setFocused(static_cast<int>(i) == index);

    const nxui::Vec2 centre = worldToScreen(m_pedestals[index]->position());
    m_handPos = {centre.x + 14.0f, centre.y - 22.0f};
    if (changed && playSfx && m_navigateSfxCb) m_navigateSfxCb();
}

void WaraWaraPlazaScreen::updateHandSelection() {
    int best = -1;
    float bestDistSq = 86.0f * 86.0f;
    for (size_t i = 0; i < m_pedestals.size(); ++i) {
        const nxui::Vec2 p = worldToScreen(m_pedestals[i]->position());
        const float dx = p.x - m_handPos.x;
        const float dy = (p.y - 24.0f) - m_handPos.y;
        const float distSq = dx * dx + dy * dy;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = static_cast<int>(i);
        }
    }

    if (best != m_focusedPedestalIndex) {
        m_focusedPedestalIndex = best;
        for (size_t i = 0; i < m_pedestals.size(); ++i)
            m_pedestals[i]->setFocused(static_cast<int>(i) == best);
        if (best >= 0 && m_navigateSfxCb) m_navigateSfxCb();
    }
}

int WaraWaraPlazaScreen::findDirectionalPedestal(int fromIndex, const nxui::Vec2& direction) const {
    if (m_pedestals.empty()) return -1;
    if (fromIndex < 0 || fromIndex >= static_cast<int>(m_pedestals.size())) {
        float best = 1e30f;
        int index = 0;
        for (size_t i = 0; i < m_pedestals.size(); ++i) {
            const nxui::Vec2 p = worldToScreen(m_pedestals[i]->position());
            const float dx = p.x - m_handPos.x;
            const float dy = p.y - m_handPos.y;
            const float d = dx * dx + dy * dy;
            if (d < best) { best = d; index = static_cast<int>(i); }
        }
        return index;
    }

    const nxui::Vec2 origin = worldToScreen(m_pedestals[fromIndex]->position());
    float bestScore = 1e30f;
    int best = -1;
    for (size_t i = 0; i < m_pedestals.size(); ++i) {
        if (static_cast<int>(i) == fromIndex) continue;
        const nxui::Vec2 p = worldToScreen(m_pedestals[i]->position());
        const float dx = p.x - origin.x;
        const float dy = p.y - origin.y;
        const float forward = dx * direction.x + dy * direction.y;
        if (forward <= 8.0f) continue;
        const float side = std::abs(dx * direction.y - dy * direction.x);
        const float score = forward + side * 2.8f;
        if (score < bestScore) { bestScore = score; best = static_cast<int>(i); }
    }
    return best;
}

void WaraWaraPlazaScreen::activateHandTarget() {
    if (m_focusedPedestalIndex >= 0 &&
        m_focusedPedestalIndex < static_cast<int>(m_pedestals.size())) {
        const uint64_t titleId = m_pedestals[m_focusedPedestalIndex]->data().titleId;
        if (titleId != 0 && m_launchGameCb) {
            if (m_activateSfxCb) m_activateSfxCb();
            m_launchGameCb(titleId);
            return;
        }
        // Presentation-only fallback communities cannot launch an application;
        // let A talk to their gathering instead of silently doing nothing.
        const nxui::Vec2 centre = m_pedestals[m_focusedPedestalIndex]->position();
        MiiFigure* nearest = nullptr;
        float nearestSq = 1e30f;
        for (const auto& mii : m_miis) {
            const float dx = mii->position().x - centre.x;
            const float dy = mii->position().y - centre.y;
            const float d = dx * dx + dy * dy;
            if (d < nearestSq) { nearestSq = d; nearest = mii.get(); }
        }
        if (nearest) {
            if (m_activateSfxCb) m_activateSfxCb();
            interactWithMii(nearest);
            return;
        }
    }
    interactWithNearestVisibleMii();
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

        // Check if tapping a community through the same transformed view used
        // for rendering. First tap selects; a second tap launches.
        for (size_t i = 0; i < m_pedestals.size(); ++i) {
            const nxui::Vec2 p = worldToScreen(m_pedestals[i]->position());
            const float radius = 82.0f * m_zoom * m_pedestals[i]->scale();
            const float dx = tx - p.x;
            const float dy = ty - (p.y - 28.0f * m_zoom);
            if (dx * dx + dy * dy <= radius * radius) {
                const bool alreadySelected = m_focusedPedestalIndex == static_cast<int>(i);
                moveHandToPedestal(static_cast<int>(i), true);
                if (alreadySelected) activateHandTarget();
                return true;
            }
        }

        m_isDraggingTouch = true;
        return true;
    }

    if (input.isTouching() && m_isDraggingTouch) {
        m_handPos = {tx, ty};
        updateHandSelection();
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

void WaraWaraPlazaScreen::drawHandCursor(nxui::Renderer& ren) const {
    // Primitive-built Wii U-style white glove: blue shadow/outline, extended
    // index finger, rounded palm and thumb. Keeping it procedural avoids a new
    // texture dependency and stays crisp throughout the zoom range.
    const float pulse = 1.0f + std::sin(m_handPulse * 4.2f) * 0.035f;
    const nxui::Vec2 p = m_handPos;
    const nxui::Color outline(0.05f, 0.50f, 0.88f, 0.98f * m_fadeAlpha);
    const nxui::Color white(0.98f, 0.99f, 1.0f, m_fadeAlpha);
    const nxui::Color shade(0.76f, 0.91f, 1.0f, m_fadeAlpha);

    ren.drawRoundedRect({p.x - 8.0f * pulse, p.y - 43.0f * pulse,
                         19.0f * pulse, 48.0f * pulse}, outline, 9.0f * pulse);
    ren.drawRoundedRect({p.x - 11.0f * pulse, p.y - 2.0f * pulse,
                         35.0f * pulse, 30.0f * pulse}, outline, 12.0f * pulse);
    ren.drawRoundedRect({p.x + 15.0f * pulse, p.y + 1.0f * pulse,
                         18.0f * pulse, 13.0f * pulse}, outline, 6.0f * pulse);
    ren.drawTriangle({p.x + 23.0f * pulse, p.y + 7.0f * pulse},
                     {p.x + 33.0f * pulse, p.y + 13.0f * pulse},
                     {p.x + 20.0f * pulse, p.y + 17.0f * pulse}, outline);

    ren.drawRoundedRect({p.x - 4.0f * pulse, p.y - 39.0f * pulse,
                         11.0f * pulse, 43.0f * pulse}, white, 5.0f * pulse);
    ren.drawRoundedRect({p.x - 7.0f * pulse, p.y + 1.0f * pulse,
                         27.0f * pulse, 23.0f * pulse}, white, 9.0f * pulse);
    ren.drawRoundedRect({p.x + 15.0f * pulse, p.y + 5.0f * pulse,
                         13.0f * pulse, 7.0f * pulse}, white, 3.5f * pulse);
    ren.drawRoundedRect({p.x - 1.0f * pulse, p.y + 18.0f * pulse,
                         17.0f * pulse, 10.0f * pulse}, shade, 4.0f * pulse);
}

void WaraWaraPlazaScreen::render(nxui::Renderer& ren) {
    if (m_fadeAlpha <= 0.001f) return;

    // Report what the scene currently holds. The dimming artifact grows more
    // frequent the longer Plaza stays open, so the population has to be visible
    // per frame for "something accumulates" to be checked rather than assumed.
    // Speech bubbles are counted from the Miis that currently own one.
    if (ren.drawJournalEnabled()) {
        uint32_t bubbles = 0;
        for (const auto& mii : m_miis)
            if (mii && mii->hasSpeechBubble()) ++bubbles;
        ren.setPlazaCounts((uint32_t)m_miis.size(),
                           (uint32_t)m_pedestals.size(), bubbles);
    }

    // 1. Draw Plaza Sky & Floor
    {
        const nxui::Renderer::DrawTagScope tag{ren, "plaza.floor"};
        drawPlazaFloor(ren);
    }

    // 2. Render a unified depth-sorted radial scene. Pedestals and Miis share
    // one Y order so a foreground community correctly occludes rear figures.
    struct SceneItem {
        float y;
        bool pedestal;
        size_t index;
    };
    std::vector<SceneItem> scene;
    scene.reserve(m_pedestals.size() + m_miis.size());
    for (size_t i = 0; i < m_pedestals.size(); ++i)
        scene.push_back({m_pedestals[i]->position().y, true, i});
    for (size_t i = 0; i < m_miis.size(); ++i)
        scene.push_back({m_miis[i]->position().y, false, i});
    std::stable_sort(scene.begin(), scene.end(), [](const SceneItem& a, const SceneItem& b) {
        return a.y < b.y;
    });

    for (const auto& item : scene) {
        if (item.pedestal) {
            const nxui::Renderer::DrawTagScope tag{ren, "plaza.pedestal.radial"};
            m_pedestals[item.index]->render(
                ren, m_fontNormal, m_fontSmall, m_cameraX, m_zoom, {640.0f, 400.0f});
        } else {
            const bool far = m_miis[item.index]->position().y < 420.0f;
            const nxui::Renderer::DrawTagScope tag{
                ren, far ? "plaza.mii.far" : "plaza.mii.near"};
            m_miis[item.index]->render(
                ren, m_fontNormal, m_fontSmall, m_cameraX, m_zoom, {640.0f, 400.0f});
        }
    }
    // 3. Header & Navigation UI
    {
        const nxui::Renderer::DrawTagScope tag{ren, "plaza.header"};
        drawHeader(ren);
    }
    drawHandCursor(ren);
}

} // namespace warawara
