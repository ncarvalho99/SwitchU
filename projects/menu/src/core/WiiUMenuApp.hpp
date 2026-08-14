#pragma once
#include <nxui/Activity.hpp>
#include <nxui/Application.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/core/I18n.hpp>
#include <nxui/Theme.hpp>
#include "widgets/IconGrid.hpp"
#include "core/GridModel.hpp"
#include "widgets/SelectionCursor.hpp"
#include "widgets/WaraWaraBackground.hpp"
#include "widgets/GameArtworkBackdrop.hpp"
#include "widgets/DateTimeWidget.hpp"
#include "widgets/BatteryWidget.hpp"
#include "widgets/TitlePillWidget.hpp"
#include "core/AudioManager.hpp"
#include "core/AccessibilityManager.hpp"
#include "widgets/LaunchAnimation.hpp"
#include "widgets/OverlayDialog.hpp"
#include "widgets/ProgressDialog.hpp"
#include "widgets/AppletButton.hpp"
#include "widgets/PageIndicator.hpp"
#include "widgets/UserAvatarButton.hpp"
#include "settings/SettingsScreen.hpp"
#include "themeshop/ThemeShopScreen.hpp"
#include "gallery/GameGalleryScreen.hpp"
#include "details/GameDetailsScreen.hpp"
#include "mods/GameModsScreen.hpp"
#include "gallery/GameArtworkStore.hpp"
#include "core/Config.hpp"
#include "core/ThemePreset.hpp"
#include "sidebar/SidebarManager.hpp"
#include "launcher/AppletLauncher.hpp"
#include "launcher/AppListLoader.hpp"
#include "launcher/IconStreamer.hpp"
#include "core/SystemMessages.hpp"
#ifdef SWITCHU_DEBUG_UI
#include "debug/DebugImGuiOverlay.hpp"
#endif
#include <nxui/widgets/Background.hpp>
#include <nxui/widgets/Box.hpp>
#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <future>
#include <switch.h>
#ifdef SWITCHU_MENU
#include <switchu/smi_protocol.hpp>
#endif


#ifdef SWITCHU_HOMEBREW
static constexpr const char* SD_ASSETS = "romfs:";
#else
static constexpr const char* SD_ASSETS = "sdmc:/switch/SwitchU";
#endif

class WiiUMenuApp : public nxui::Activity {
public:
    WiiUMenuApp();
    ~WiiUMenuApp();

    void setTutorialStartupFade(bool enabled);

#ifdef SWITCHU_MENU
    void setStartupStatus(uint64_t suspendedTitleId, bool appRunning);
#endif

    bool onCreate() override;
    void onDestroy() override;
    void onUpdate(float dt) override;
    void onRender(nxui::Renderer& ren) override;

    nxui::Widget* focusRoot() override;

private:
    struct GridLayoutMetrics {
        float cellW = 150.f;
        float cellH = 150.f;
        float padX = 20.f;
        float padY = 16.f;
    };

    void loadResources();
    GridLayoutMetrics computeGridLayoutMetrics() const;
    void reflowHomeGrid();
    void buildGrid();
    void applyGlassSharpness(float sharpness);
    // The + menu on a focused icon: what the stock home menu offers, minus
    // the entries that would need a daemon round trip to answer.
    // Cycles hand-made order, A-Z, most recently opened. The hint bar shows
    // which one is in force rather than just naming the button.
    void cycleSortMode();
    std::string sortModeLabel() const;
    void showIconOptions();
    // Homebrew, forwarders and ports: the dossier has nothing to show for them,
    // so + offers the single action that applies.
    void showNonGameOptions(std::uint64_t titleId, const std::string& title);
    void showGameOptionsMenu(std::uint64_t titleId, const std::string& title);
    void showGameCustomizeMenu(std::uint64_t titleId, const std::string& title);
    void showGameArtworkStatus(std::uint64_t titleId, const std::string& title);
    void showGameArtworkRestoreMenu(std::uint64_t titleId, const std::string& title);
    void restoreGameArtworkFromOptions(std::uint64_t titleId, const std::string& title,
                                       gallery::ArtworkKind kind);
    void showSoftwareInformation(std::uint64_t titleId, const std::string& title);
    void showGameGallery(std::uint64_t titleId, const std::string& title);
    void showGameDetails(std::uint64_t titleId, const std::string& title,
                         nxui::Texture* liveCover = nullptr);
    void showGameMods(std::uint64_t titleId, const std::string& title);
    void confirmDeleteGameMod();
    void confirmGameArtwork(std::uint64_t titleId, const std::string& title,
                            GameGalleryClient::Category category,
                            GameGalleryClient::Asset asset,
                            std::shared_ptr<const std::vector<std::uint8_t>> bytes);
    void startGameArtworkSave(std::uint64_t titleId, GameGalleryClient::Category category,
                              GameGalleryClient::Asset asset,
                              std::shared_ptr<const std::vector<std::uint8_t>> bytes);
    void confirmRestoreGameArtwork(std::uint64_t titleId, const std::string& title,
                                   GameGalleryClient::Category category);
    void startGameArtworkRestore(std::uint64_t titleId, GameGalleryClient::Category category);
    void syncGameArtworkSave();
    void refreshGameArtworkBackdrop(std::uint64_t titleId);
    void confirmDeleteSoftware(std::uint64_t titleId, const std::string& title);
    void buildUserAvatarBar();
    void applyTheme();
    void applyThemeResources(const ThemePreset& preset);
    void applyThemeMusic(const std::vector<std::string>& tracks);
    // Faixas que o tema atual traz. O preset de som carrega em outra thread e
    // chegava depois, sobrescrevendo o que o tema tinha posto -- entao quem
    // carrega o preset precisa saber que nao deve tocar na musica.
    std::vector<std::string> m_themeMusicTracks;
    void applyUiLanguage();
    void rebuildThemeFromColors();
    ThemePreset buildEffectiveThemePreset();
    std::string resolveThemeAssetPath(const ThemePreset& preset, const std::string& rawPath) const;
    ThemePreset* findPresetPtr(const std::string& name);
    void deletePreset(const std::string& presetId);
    void updateCursor();
    struct ActionHint {
        std::string icon;
        std::string label;
    };
    std::vector<ActionHint> buildActionHints();
    void renderActionHintBar(nxui::Renderer& ren);
    int findTitleIndex(uint64_t titleId) const;
    bool focusTitle(uint64_t titleId);
    void markSuspendedIcon(uint64_t titleId);
    void closeActiveOverlays();
    void handleTouch();
    std::shared_ptr<GlossyIcon> makeIcon(const AppEntry& entry);
    void wireFocusCallback();
    void wireGlobalActions();
    void toggleAccessibilitySpeech();
    bool handleAccessibilityToggleCombo();
    void handleSortShortcutRelease(float dt);
    // Longer than a deliberate tap, far shorter than the hold used to reach
    // Sphaira, so the two gestures never get confused for one another.
    static constexpr float kSortShortcutTapSeconds = 0.35f;
    bool isCurrentFocusableWidget(nxui::Widget* w) const;
    std::string accessibilityPositionFor(nxui::Widget* w) const;
    void createSettings();
    void createThemeShop();
    void createGameGallery();
    void createGameDetails();
    void reloadThemePresets();
    void refreshThemeShopState();
    std::vector<ThemeShopScreen::ThemeShopEntry> buildThemeShopEntries();
    void startThemePackageTransfer(const ThemeCatalogClient::Entry& entry, bool installMode);
    void syncThemePackageTransfer();
    void activateThemePreset(ThemePreset* preset, bool applyBundledSound);
    std::string resolveSoundPresetId(const std::string& preset) const;
    void loadSoundPreset(const std::string& preset);
    void changeSoundPreset(const std::string& preset);
    std::vector<std::string> scanAvailablePresets();
    void loadMenuLayout();
    void saveMenuLayout();
    void quiesceWritersForPowerAction();
    void applyMenuLayoutToPending(std::vector<PendingApp>& apps);
    void startEditGhost(GlossyIcon* sourceIcon);
    void stopEditGhost();
    void updateEditGhost(float dt);
    bool commitEditModePlacement();
    bool moveFocusedIcon(nxui::FocusDirection dir);
    void enterEditMode();
    void exitEditMode();
    void bindEditActions(GlossyIcon* icon);
    void unbindEditActions();
    bool isEditableIcon(nxui::Widget* w) const;
    void announceFocusedWidget(nxui::Widget* w);
    std::string accessibilityContextFor(nxui::Widget* w) const;
    std::string accessibilityActionsFor(nxui::Widget* w) const;

#ifdef SWITCHU_MENU
    void refreshAppList();
    void finalizeRefresh();
    void handleSystemAction(SysAction a);
#endif

    nxui::Font  m_fontNormal;
    nxui::Font  m_fontSmall;
    nxui::Font  m_fontIcons;

    GridModel    m_model;
    nxui::Theme  m_theme;

    std::string              m_activePresetName = "Default Dark";
    ThemeColorSet            m_activeColors;
    nxui::ThemeMode          m_activeMode = nxui::ThemeMode::Light;
    std::vector<ThemePreset> m_allPresets;
    ThemePreset              m_effectivePreset;

    std::shared_ptr<WaraWaraBackground> m_background;
    std::shared_ptr<GameArtworkBackdrop> m_gameArtworkBackdrop;
    std::shared_ptr<IconGrid>          m_grid;
    std::shared_ptr<SelectionCursor>   m_cursor;
    std::shared_ptr<SelectionCursor>   m_pointerCursor;
    std::shared_ptr<DateTimeWidget>    m_clock;
    std::shared_ptr<BatteryWidget>     m_battery;
    std::shared_ptr<TitlePillWidget>   m_titlePill;
    std::shared_ptr<PageIndicator>     m_pageIndicator;
    std::shared_ptr<LaunchAnimation>   m_launchAnim;
    std::shared_ptr<OverlayDialog>     m_userSelect;
    std::shared_ptr<OverlayDialog>     m_dialog;
    std::shared_ptr<ProgressDialog>    m_progressDialog;
    std::shared_ptr<SettingsScreen>    m_settings;
    std::shared_ptr<ThemeShopScreen>   m_themeShop;
    std::shared_ptr<GameGalleryScreen> m_gameGallery;
    std::shared_ptr<GameDetailsScreen> m_gameDetails;
    std::shared_ptr<GameModsScreen>    m_gameMods;

    nxui::Texture m_gameCardTex;

    std::shared_ptr<nxui::Box> m_bgLayer;
    std::shared_ptr<nxui::Box> m_contentLayer;
    std::shared_ptr<nxui::Box> m_overlayLayer;
    std::shared_ptr<nxui::Box> m_topHud;
    std::shared_ptr<nxui::Box> m_leftSidebar;
    std::shared_ptr<nxui::Box> m_rightSidebar;
    std::shared_ptr<nxui::Box> m_userAvatarBar;
    std::vector<std::shared_ptr<UserAvatarButton>> m_userAvatarButtons;

    AudioManager m_audio;
    AccessibilityManager m_accessibility;
    std::future<void>    m_audioFuture;
    // The config save runs on the thread pool. A power request has to wait on
    // it: the corruption is intermittent at roughly one reboot in three, which
    // is what losing a race with an in-flight write looks like.
    std::future<void>    m_configSaveFuture;
    bool                 m_audioStarted = false;
    std::vector<std::string> m_availablePresets;
    bool                 m_presetChangePending = false;
    std::string          m_loadedSoundPreset;
    std::string          m_pendingSoundPreset;

    struct ThemePackageTransferShared {
        std::mutex mutex;
        ThemeTransferState state;
        std::string themeId;
        bool installMode = false;
        std::string destinationPath;
        std::uint64_t revision = 0;
    };

    struct GameArtworkSaveShared {
        std::mutex mutex;
        gallery::ArtworkSaveResult result;
    };

    nxui::ThreadPool m_threadPool{2};
    SidebarManager  m_sidebar;
    AppletLauncher  m_launcher;
    AppListLoader   m_appLoader;
    IconStreamer    m_iconStreamer;
    SystemMessages  m_sysMsg;

    bool m_showDebugOverlay  = false;
#ifdef SWITCHU_DEBUG_UI
    std::unique_ptr<DebugImGuiOverlay> m_debugOverlay;
#endif
    bool m_showWireframe     = false;
    bool m_editMode          = false;
    int  m_editSourceIndex   = -1;
    std::string m_editHeldTitle;
    GlossyIcon* m_editBoundIcon = nullptr;
    GlossyIcon* m_editSourceIcon = nullptr;
    std::shared_ptr<GlossyIcon> m_editGhostIcon;
    nxui::Rect m_editGhostTargetRect {0.f, 0.f, 0.f, 0.f};
    float m_editGhostPulse = 0.f;
    std::vector<uint64_t> m_layoutSlots;
    bool m_layoutDirty = false;
    // Frames the log stays unbuffered for after the run loop starts, so a hang
    // in early-frame work (deferred asset uploads) still reaches the SD card.
    int m_logImmediateFrames = 300;
    float m_perfAccumDt = 0.f;
    int   m_perfFrames = 0;
    float m_perfWorstDt = 0.f;
    bool  m_probeSceneHidden = false;

    // Vale so para a amostragem de memoria: a loja de temas e onde o menu saiu
    // duas vezes sem deixar rastro, e medir o tempo todo encheria o log.
    bool  inThemeShopForMemorySampling() const;
    float m_memSampleTimer = 0.f;

    int  m_touchHitIndex     = -1;
    bool m_touchOnFocused    = false;
    bool m_touchEditDragActive = false;
    UserAvatarButton* m_touchAvatarTarget = nullptr;
    bool m_touchAvatarWasFocused = false;
    int  m_deferredRefreshFrames = 0;
    bool m_refreshQueued         = false;
    int  m_refreshCooldownFrames = 0;
    bool m_asyncRefreshPending   = false;
    int  m_refreshPrevPage       = 0;

    AppConfig m_config;
    bool m_settingsNeedRefresh        = false;
    std::string m_loadedRegularFontPath;
    std::string m_loadedSmallFontPath;
    std::string m_loadedGameCardPath;
    std::string m_loadedBackgroundImagePath;
    bool m_backgroundImageLoaded      = false;
    bool m_forceThemeResourceReload   = false;
    nxui::Widget* m_dialogReturnFocus = nullptr;
    // The Gallery is launched from a close-on-press dialog. Its return focus
    // must outlive that dialog's own return-focus handoff.
    nxui::Widget* m_gameGalleryReturnFocus = nullptr;
    nxui::Widget* m_gameDetailsReturnFocus = nullptr;
    bool m_dialogWasActive            = false;
    bool m_suppressNextNavigateSfx    = false;
    bool m_pendingNetConnect          = false;
    int  m_deferredBluetoothInitFrames = 0;
    int  m_deferredInitialAssetFrames = 0;
    std::future<void> m_themePackageTransferFuture;
    std::future<void> m_gameArtworkSaveFuture;
    std::shared_ptr<GameArtworkSaveShared> m_gameArtworkSave;
    std::shared_ptr<ThemePackageTransferShared> m_themePackageTransfer;
    std::uint64_t m_themePackageTransferUiRevision = 0;
    std::uint64_t m_themePackageTransferHandledRevision = 0;
    int m_themeRenderDebugFrames = 0;

    float m_returnFadeTimer = 0.f;
    float m_tutorialStartupFadeTimer = 0.f;
    bool  m_tutorialStartupFade = false;
    bool m_hintPanelInitialized = false;
    bool m_accessibilityToggleComboHeld = false;
    // Set while R is held so a release only sorts when the press began here,
    // and not when R was already down on the way back from another screen.
    bool m_sortShortcutArmed = false;
    float m_sortShortcutHeld = 0.f;
    bool m_plusExitPending = false;
    float m_plusExitPendingTimer = 0.f;
    nxui::AnimatedFloat m_hintPanelW{0.f};
    nxui::AnimatedFloat m_hintPanelH{0.f};
    nxui::AnimatedFloat m_hintContentReveal{1.f};
    std::string m_hintSignature;
    static constexpr float kReturnFadeInDur = 0.22f;
    static constexpr float kTutorialStartupFadeDur = 0.34f;
};
