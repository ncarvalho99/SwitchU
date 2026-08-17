#pragma once

#include "ThemeCatalogClient.hpp"
#include <unordered_set>
#include "settings/TabbedOverlayScreen.hpp"

#include <nxui/core/Texture.hpp>

#include <cstdint>
#include <algorithm>
#include <future>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace themeshop::tabs {
class InstalledTab;
class CommunityTab;
class AnimatedTab;
class OptionsTab;
class UpdateTab;
}

namespace nxui {
class GpuDevice;
class Renderer;
}

class ThemeShopScreen : public TabbedOverlayScreen {
public:
    struct ThemeShopEntry {
        std::string id;
        std::string name;
        std::string author;
        std::string version;
        std::string source;
        std::string soundPreset;
        std::string coverPath;
        // De onde o tema foi instalado. Vazio para os embutidos, que vivem no
        // romfs do proprio build e nao ocupam nada do cartao.
        std::string installPath;
        bool active = false;
        bool removable = false;
    };

    ThemeShopScreen();
    ~ThemeShopScreen() override = default;

    void onMusicEnabledChange(BoolCb cb) { m_musicEnabledCb = std::move(cb); }
    void onUpdateCheck(std::function<void()> cb)   { m_updateCheckCb = std::move(cb); }
    void onUpdateInstall(std::function<void()> cb) { m_updateInstallCb = std::move(cb); }
    void onUpdateRestart(std::function<void()> cb) { m_updateRestartCb = std::move(cb); }
    // Called by the app whenever the update state changes, so the tab redraws
    // with the answer instead of polling for it.
    void setUpdateState(std::string installed, std::string available,
                        std::string status, bool busy,
                        std::string latestVersion, std::string latestNotes,
                        bool restartPending) {
        m_updateInstalledVersion = std::move(installed);
        m_updateAvailableVersion = std::move(available);
        m_updateStatusText = std::move(status);
        m_updateBusy = busy;
        m_updateLatestVersion = std::move(latestVersion);
        m_updateLatestNotes = std::move(latestNotes);
        m_updateRestartPending = restartPending;
        rebuildCurrentTab();
    }
    void onReleaseNotes(std::function<void()> cb) { m_releaseNotesCb = std::move(cb); }
    void onMusicVolumeChange(FloatCb cb) { m_musicVolumeCb = std::move(cb); }
    void onSfxVolumeChange(FloatCb cb)   { m_sfxVolumeCb = std::move(cb); }
    // The three appearance controls also live under Settings > Display. They
    // are repeated here because this is the screen someone is on while judging
    // how a theme looks, and walking back out to change them breaks that.
    void onGlassSharpnessChange(FloatCb cb)  { m_glassSharpnessCb = std::move(cb); }
    void onBackgroundSpeedChange(FloatCb cb) { m_backgroundSpeedCb = std::move(cb); }
    void onBackgroundBlurChange(FloatCb cb)  { m_backgroundBlurCb = std::move(cb); }
    void onGridColumnsChange(IntCb cb)   { m_gridColumnsCb = std::move(cb); }
    void onGridRowsChange(IntCb cb)      { m_gridRowsCb = std::move(cb); }
    void onNextTrack(VoidCb cb)          { m_nextTrackCb = std::move(cb); }
    void onThemeShopApply(StringCb cb)   { m_themeShopApplyCb = std::move(cb); }
    void onThemeShopDelete(StringCb cb)  { m_themeShopDeleteCb = std::move(cb); }
    void onThemeShopDownload(StringCb cb) { m_themeShopDownloadCb = std::move(cb); }
    void onThemeShopDownloadInstall(StringCb cb) { m_themeShopDownloadInstallCb = std::move(cb); }
    void onNetConnectRequest(VoidCb cb)  { m_netConnectCb = std::move(cb); }

    void setMusicState(bool enabled, float musicVol, float sfxVol) {
        m_musicEnabled = enabled;
        m_musicVolume = musicVol;
        m_sfxVolume = sfxVol;
    }
    void setAppearanceState(float glassSharpness, float backgroundSpeed, float backgroundBlur) {
        m_glassSharpness = std::clamp(glassSharpness, 0.f, 1.f);
        m_backgroundSpeed = std::clamp(backgroundSpeed, 0.f, 1.f);
        m_backgroundBlur = std::clamp(backgroundBlur, 0.f, 1.f);
    }
    void setGridLayoutState(int columns, int rows) {
        m_gridColumns = std::clamp(columns, 3, 8);
        m_gridRows = std::clamp(rows, 2, 5);
    }

    void setThreadPool(nxui::ThreadPool* pool);
    void setRenderContext(nxui::GpuDevice* gpu, nxui::Renderer* renderer);
    void refreshCommunityCatalog();

    // Vira a página do catálogo, de onde quer que o foco esteja.
    //
    // Chegar até os botões Anterior e Próxima é descer a grade inteira, e num
    // catálogo de dezenas de temas virar página é o gesto mais repetido da
    // tela; os gatilhos L e R fazem isso sem tirar o foco do lugar. Devolve
    // true quando a página mudou de fato, para o som tocar só aí.
    //
    // Recusa com o detalhe aberto: ali L e R já percorrem as capturas.
    bool stepCataloguePage(int delta);
    void requestRenderDiagnostics(int frames = 6) {
        if (frames > m_renderDebugFrames)
            m_renderDebugFrames = frames;
    }

    void setThemeShopState(const std::vector<ThemeShopEntry>& entries,
                           const std::string& activeId);
    void setPackageTransferState(const ThemeTransferState& state,
                                 std::string themeId,
                                 bool installMode);
    const ThemeShopEntry* selectedThemeShopEntry() const;
    const ThemeCatalogClient::Entry* selectedCommunityThemeEntry() const;
    const ThemeCatalogClient::Entry* findCommunityThemeEntry(const std::string& themeId) const;
    const std::string& communityCatalogUrl() const {
        return m_catalogClient.catalogUrl();
    }

protected:
    void buildTabs() override;
    bool usesCustomContentLayout() const override { return m_tabIndex < 3; }
    void drawCustomContent(nxui::Renderer& ren, const nxui::Rect& panel, const nxui::Rect& content, float opacity) override;
    void updateCustomContent(float dt) override;
    bool handleCustomPressA() override;
    bool handleCustomPressB() override;
    bool handleCustomPressX() override;
    bool handleCustomNavUp() override;
    bool handleCustomNavDown() override;
    bool handleCustomNavLeft() override;
    bool handleCustomNavRight() override;
    bool handleCustomTouch(nxui::Input& input, const nxui::Rect& panel, const nxui::Rect& tabs, const nxui::Rect& content) override;
    void currentAccessibilityParts(std::string& context,
                                   std::string& position,
                                   std::string& summary,
                                   bool& forceRepeat) const override;
    std::string currentAccessibilitySummary() const override;

private:
    friend class themeshop::tabs::InstalledTab;
    friend class themeshop::tabs::CommunityTab;
    friend class themeshop::tabs::AnimatedTab;
    friend class themeshop::tabs::OptionsTab;
    friend class themeshop::tabs::UpdateTab;

    enum class PreviewPhase {
        Idle,
        Loading,
        Downloaded,
        Ready,
        Failed,
    };

    struct PreviewImageState {
        std::mutex mutex;
        PreviewPhase phase = PreviewPhase::Idle;
        int failureCount = 0;
        bool cancelled = false;
        std::string url;
        std::vector<std::uint8_t> bytes;
        std::future<void> future;
        nxui::Texture texture;
    };

    enum class ThemeTouchTarget {
        None,
        Search,
        Refresh,
        PagePrev,
        PageNext,
        GridCard,
        DetailPreviewControl,
        DetailButton,
        FullscreenPreview,
        DetailBackdrop,
    };

    enum class DetailFocusArea {
        Preview,
        Buttons,
    };

    enum class ContentFocusArea {
        Grid,
        Header,
        Pager,
    };

    bool pollCommunityCatalog();
    void syncCommunityCatalog(const ThemeCatalogClient::Snapshot& snapshot);
    bool isCommunityTab() const;
    bool isAnimatedTab() const;
    // One line summarising the open tab's whole catalogue: how many themes it
    // has, and what they cost to download and to keep.
    std::string communityCatalogueTotals() const;

    // O mesmo resumo para o que já está no console: quantos temas e quanto do
    // cartão eles ocupam.
    std::string installedThemeTotals() const;
    // O que um tema instalado ocupa, ou 0 enquanto a medição não voltou.
    std::uint64_t installedThemeBytes(const std::string& installPath) const;
    // Põe na fila a medição dos temas que ainda não têm tamanho. Percorrer a
    // pasta de um tema animado é centenas de arquivos no cartão, então isso
    // nunca acontece na thread que desenha.
    void measureInstalledThemes();
    int currentEntryCount() const;
    int currentSelectedIndex() const;
    void setCurrentSelectedIndex(int idx);
    int currentPage() const;
    int pageCount() const;
    void setCurrentPage(int page);
    void stepPage(int delta);
    void applySearchFilter();
    bool promptSearchQuery();
    int detailScreenshotCount() const;
    std::string currentDetailCommunityPreviewPath() const;
    void clampDetailScreenshotIndex();
    void stepDetailScreenshot(int delta);
    int& currentScrollRowRef();
    void ensureSelectionVisible();
    void openDetail();
    void closeDetail();
    int detailButtonCount() const;
    void activateDetailButton(int buttonIndex);
    int hitTestGridCard(const nxui::Rect& content, float x, float y) const;
    std::string resolveCommunityPreviewUrl(const std::string& previewPath) const;
    std::string resolveCommunityPreviewUrl(const ThemeCatalogClient::Entry& entry) const;
    std::shared_ptr<PreviewImageState> communityPreviewState(const std::string& previewPath) const;
    std::shared_ptr<PreviewImageState> communityPreviewState(const ThemeCatalogClient::Entry& entry) const;
    PreviewPhase communityPreviewPhase(const std::string& previewPath) const;
    PreviewPhase communityPreviewPhase(const ThemeCatalogClient::Entry& entry) const;
    const nxui::Texture* communityPreviewTexture(const std::string& previewPath) const;
    const nxui::Texture* communityPreviewTexture(const ThemeCatalogClient::Entry& entry) const;
    PreviewPhase installedPreviewPhase(const std::string& previewPath) const;
    const nxui::Texture* installedPreviewTexture(const std::string& previewPath) const;
    void primeInstalledPreview(const std::string& previewPath);
    void primeCommunityPreview(const std::string& previewPath);
    void primeCommunityPreview(const ThemeCatalogClient::Entry& entry);
    void primeVisibleCommunityPreviews();
    void syncFinishedCommunityPreviewLoads();
    void clearCommunityPreviewCache();
    std::unordered_set<std::string> wantedCommunityPreviewUrls() const;
    void trimCommunityPreviewCache();

    BoolCb m_musicEnabledCb;
    FloatCb m_musicVolumeCb;
    FloatCb m_sfxVolumeCb;
    FloatCb m_glassSharpnessCb;
    FloatCb m_backgroundSpeedCb;
    FloatCb m_backgroundBlurCb;
    IntCb m_gridColumnsCb;
    IntCb m_gridRowsCb;
    VoidCb m_nextTrackCb;
    StringCb m_themeShopApplyCb;
    StringCb m_themeShopDeleteCb;
    StringCb m_themeShopDownloadCb;
    StringCb m_themeShopDownloadInstallCb;
    VoidCb m_netConnectCb;

    bool m_musicEnabled = true;
    // What the Update tab shows. The app owns the check itself and pushes the
    // outcome here, so the tab stays a view rather than a second client.
    std::string m_updateInstalledVersion;
    std::string m_updateAvailableVersion;
    std::string m_updateStatusText;
    bool  m_updateBusy = false;
    // Um pacote já baixado e conferido, esperando o reinício que o aplica. Nesse
    // estado não há o que baixar de novo, e era justamente isso que fazia a aba
    // oferecer a mesma atualização sem fim.
    bool  m_updateRestartPending = false;
    std::function<void()> m_updateCheckCb;
    std::function<void()> m_updateInstallCb;
    std::function<void()> m_updateRestartCb;
    std::function<void()> m_releaseNotesCb;
    std::string m_updateLatestVersion;
    std::string m_updateLatestNotes;
    float m_musicVolume = 0.4f;
    float m_sfxVolume = 0.7f;
    float m_glassSharpness = 0.4f;
    float m_backgroundSpeed = 0.5f;
    float m_backgroundBlur = 0.f;
    int m_gridColumns = 5;
    int m_gridRows = 3;
    std::string m_searchQuery;
    std::vector<ThemeShopEntry> m_allThemeShopEntries;
    std::vector<ThemeShopEntry> m_themeShopEntries;
    std::string m_themeShopSelectedId;

    // Tamanhos medidos por caminho de instalação. Compartilhado com as threads
    // de trabalho que fazem a medição, daí o mutex; a leitura acontece durante
    // o desenho e é só uma consulta.
    struct InstalledSizes {
        mutable std::mutex mutex;
        std::unordered_map<std::string, std::uint64_t> bytes;
        std::unordered_set<std::string> inFlight;
    };
    std::shared_ptr<InstalledSizes> m_installedSizes = std::make_shared<InstalledSizes>();

    nxui::ThreadPool* m_threadPool = nullptr;
    nxui::GpuDevice* m_gpu = nullptr;
    nxui::Renderer* m_renderer = nullptr;
    ThemeCatalogClient m_catalogClient;
    ThemeTransferState m_communityTransferState;
    ThemeTransferState m_packageTransferState;
    std::unordered_map<std::string, std::shared_ptr<PreviewImageState>> m_communityPreviewCache;
    std::unordered_map<std::string, std::shared_ptr<PreviewImageState>> m_installedPreviewCache;
    std::vector<ThemeCatalogClient::Entry> m_allCommunityEntries;
    std::vector<ThemeCatalogClient::Entry> m_communityEntries;
    std::string m_communitySelectedId;
    std::string m_packageTransferThemeId;
    bool m_packageTransferInstallMode = false;
    bool m_hasPendingCommunityPreviewWork = false;
    std::uint64_t m_communityRevision = 0;
    bool m_detailOpen = false;
    bool m_detailFullscreen = false;
    std::string m_lastLoggedDetailPath;
    ContentFocusArea m_contentFocusArea = ContentFocusArea::Grid;
    DetailFocusArea m_detailFocusArea = DetailFocusArea::Buttons;
    int m_headerButtonIndex = 0;
    int m_pageButtonIndex = 1;
    int m_detailButtonIndex = 0;
    int m_detailPreviewButtonIndex = 0;
    int m_detailScreenshotIndex = 0;
    nxui::AnimatedFloat m_detailSheetAnim{0.f};
    nxui::AnimatedFloat m_detailFullscreenAnim{0.f};
    int m_installedScrollRow = 0;
    int m_communityScrollRow = 0;
    int m_lastCustomTabIndex = -1;
    std::string m_lastPreviewPrimeKey;
    float m_previewTrimTimer = 0.f;
    ThemeTouchTarget m_themeTouchTarget = ThemeTouchTarget::None;
    int m_themeTouchIndex = -1;
    float m_themeTouchStartX = 0.f;
    float m_themeTouchStartY = 0.f;
    int m_renderDebugFrames = 0;
};
