#pragma once

#include "PlazaPedestal.hpp"
#include "MiiFigure.hpp"
#include "PlazaDialogueEngine.hpp"
#include "AnimalesePlayer.hpp"
#include "MiiAvatarManager.hpp"
#include "activity/ActivityLogManager.hpp"

#include <nxui/widgets/Widget.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/Input.hpp>
#include <nxui/core/Font.hpp>

#include <vector>
#include <memory>
#include <functional>
#include <random>

namespace warawara {

/// Dedicated full-screen WaraWara Plaza experience for SwitchU.
/// Features 10 community pedestals, 30-40 clustered and roaming procedural Miis,
/// dynamic context-aware speech bubbles, Animalese voice chatter, and smooth camera panning.
class WaraWaraPlazaScreen : public nxui::Widget {
public:
    WaraWaraPlazaScreen();
    ~WaraWaraPlazaScreen() override = default;

    void setFonts(nxui::Font* normalFont, nxui::Font* smallFont) {
        m_fontNormal = normalFont;
        m_fontSmall = smallFont;
    }

    void setServices(MiiAvatarManager* avatarMgr,
                     PlazaDialogueEngine* dialogueEngine,
                     AnimalesePlayer* animalesePlayer,
                     const switchu::activity::ActivityLogManager* activityLog) {
        m_avatarManager = avatarMgr;
        m_dialogueEngine = dialogueEngine;
        m_animalesePlayer = animalesePlayer;
        m_activityLog = activityLog;
    }

    /// Community game info for populating the 10 community pedestals.
    struct GameCommunityEntry {
        std::uint64_t titleId = 0;
        std::string title;
        std::string subtitle;
        nxui::Texture* iconTexture = nullptr;
    };

    void setupCommunities(const std::vector<GameCommunityEntry>& entries);

    void open();
    void close();
    bool isActive() const { return m_active || m_fadeAlpha > 0.001f; }
    bool isFullyVisible() const { return m_active && m_fadeAlpha >= 0.99f; }

    void update(float dt) override;
    void render(nxui::Renderer& ren) override;

    bool handleInput(const nxui::Input& input, float dt);
    bool handleTouch(const nxui::Input& input);

    void onLaunchGame(std::function<void(std::uint64_t)> cb) { m_launchGameCb = std::move(cb); }
    void onClose(std::function<void()> cb) { m_closeCb = std::move(cb); }
    void onNavigateSfx(std::function<void()> cb) { m_navigateSfxCb = std::move(cb); }
    void onActivateSfx(std::function<void()> cb) { m_activateSfxCb = std::move(cb); }

    float cameraX() const { return m_cameraX; }
    float zoom() const { return m_zoom; }

private:
    void populateMiis();
    void triggerRandomSpeechBubble();
    MiiFigure* nearestVisibleMii() const;
    bool interactWithNearestVisibleMii();
    void interactWithMii(MiiFigure* mii);
    void drawPlazaFloor(nxui::Renderer& ren) const;
    void drawHeader(nxui::Renderer& ren) const;
    void drawHandCursor(nxui::Renderer& ren) const;
    void updateHandSelection();
    void moveHandToPedestal(int index, bool playSfx);
    void selectPedestalStep(int step);
    int findDirectionalPedestal(int fromIndex, const nxui::Vec2& direction) const;
    void activateHandTarget();
    void updateCarouselPositions();
    nxui::Vec2 worldToScreen(const nxui::Vec2& world) const;

    struct MiiBinding {
        int pedestalIndex = -1; // -1 for center VIP welcoming Miis, >= 0 for community pedestal
        int slotIndex = 0;
        nxui::Vec2 localJitter{0.0f, 0.0f};
        float stateTimer = 3.0f;
    };

    bool m_active = false;
    float m_fadeAlpha = 0.0f;
    float m_time = 0.0f;

    // Camera panning (X and Y) when zoomed in, plus zoom factor
    float m_cameraX = 0.0f;
    float m_cameraY = 0.0f;
    float m_cameraTargetX = 0.0f;
    float m_cameraTargetY = 0.0f;
    float m_zoom = 1.0f;
    float m_zoomTarget = 1.0f;

    // Carousel rotation around the elliptical plaza ring
    float m_carouselAngle = 0.0f;
    float m_carouselTargetAngle = 0.0f;

    nxui::Vec2 m_lastTouchPos{0.0f, 0.0f};
    bool m_isDraggingTouch = false;

    // Pedestals, Miis, bindings, and Wii U-style hand pointer
    std::vector<std::unique_ptr<PlazaPedestal>> m_pedestals;
    std::vector<std::unique_ptr<MiiFigure>> m_miis;
    std::vector<MiiBinding> m_miiBindings;
    int m_focusedPedestalIndex = -1;
    MiiFigure* m_focusedMii = nullptr;
    nxui::Vec2 m_handPos{640.0f, 360.0f};
    float m_handPulse = 0.0f;

    // Speech bubble scheduler
    float m_speechBubbleTimer = 3.0f;
    float m_speechBubbleInterval = 4.8f;

    // External dependencies
    nxui::Font* m_fontNormal = nullptr;
    nxui::Font* m_fontSmall = nullptr;
    MiiAvatarManager* m_avatarManager = nullptr;
    PlazaDialogueEngine* m_dialogueEngine = nullptr;
    AnimalesePlayer* m_animalesePlayer = nullptr;
    const switchu::activity::ActivityLogManager* m_activityLog = nullptr;

    std::mt19937 m_rng;
    std::function<void(std::uint64_t)> m_launchGameCb;
    std::function<void()> m_closeCb;
    std::function<void()> m_navigateSfxCb;
    std::function<void()> m_activateSfxCb;
};

} // namespace warawara
