#pragma once

#include "GameMetadataClient.hpp"
#include "settings/TabbedOverlayScreen.hpp"

#include <nxui/core/Texture.hpp>

#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nxui {
class GpuDevice;
class Renderer;
class ThreadPool;
}

// Full-screen dossier reached from + > Software information. It deliberately
// keeps the game art local and fetches only descriptive metadata from SwitchU's
// own API, so an offline console still has a useful details view.
class GameDetailsScreen final : public TabbedOverlayScreen {
public:
    GameDetailsScreen();

    void setThreadPool(nxui::ThreadPool* pool) { m_pool = pool; }
    void setRenderContext(nxui::GpuDevice* gpu, nxui::Renderer* renderer) {
        m_gpu = gpu;
        m_renderer = renderer;
    }
    void openForGame(std::uint64_t titleId, std::string title, std::string searchTitle,
                     std::vector<std::uint8_t> activeCover,
                      nxui::Texture* liveCover, std::string displayVersion,
                      std::string modSummary, std::string playTime,
                      std::string metadataPlatform, bool isGamePort);
    // Resume the dossier after a child full-screen flow (Gallery) closes.
    // It preserves downloaded metadata and the selected artwork instead of
    // requesting the online dossier again.
    void resumeFromChild();
    // Applies a corrected online-lookup string (from the "Edit search title"
    // action) and re-fetches metadata with it. Leaves the displayed title
    // and everything else untouched -- only the string sent to the catalogue
    // changes.
    void updateSearchTitle(std::string searchTitle);
    bool isImageExpanded() const { return m_imageExpanded; }
    std::uint64_t titleId() const { return m_titleId; }
    const std::string& title() const { return m_title; }
    const std::string& searchTitle() const { return m_searchTitle; }
    bool isGamePort() const { return m_isGamePort; }

    using ActionCb = std::function<void()>;
    void onOpenGallery(ActionCb cb) { m_openGalleryCb = std::move(cb); }
    void onShowArtwork(ActionCb cb) { m_showArtworkCb = std::move(cb); }
    void onRestoreArtwork(ActionCb cb) { m_restoreArtworkCb = std::move(cb); }
    void onManageMods(ActionCb cb) { m_manageModsCb = std::move(cb); }
    void onDeleteSoftware(ActionCb cb) { m_deleteSoftwareCb = std::move(cb); }
    void onFolderAction(ActionCb cb) { m_folderActionCb = std::move(cb); }
    void onRemoveGamePort(ActionCb cb) { m_removeGamePortCb = std::move(cb); }
    // Offered for every title, native-id-range or not: a community port
    // packaged with a real-looking title id would otherwise never be
    // reachable through "Mark as game port" and would stay stuck querying
    // the catalogue as "nintendo-switch".
    void onMarkAsGamePort(ActionCb cb) { m_markAsGamePortCb = std::move(cb); }
    // Offered only once a title is already marked as a port: lets the player
    // correct the string sent to the online catalogue without re-picking a
    // platform.
    void onEditSearchTitle(ActionCb cb) { m_editSearchTitleCb = std::move(cb); }
    // Reads "Add to folder" or "Remove from folder" depending on where the
    // title currently lives; the owner sets it before opening.
    void setFolderActionLabel(std::string label) { m_folderActionLabel = std::move(label); }

protected:
    void buildTabs() override;
    bool usesCustomContentLayout() const override { return true; }
    bool customContentUsesPanel() const override { return true; }
    bool drawsCustomContentPanel() const override { return false; }
    bool hidesTabRail() const override { return true; }
    void drawCustomContent(nxui::Renderer& ren, const nxui::Rect& panel,
                           const nxui::Rect& content, float opacity) override;
    void updateCustomContent(float dt) override;
    bool handleCustomPressA() override;
    bool handleCustomPressB() override;
    bool handleCustomNavUp() override;
    bool handleCustomNavDown() override;
    bool handleCustomNavLeft() override;
    bool handleCustomNavRight() override;
    std::string currentAccessibilitySummary() const override;
    std::string onlineStatusMessage() const;

private:
    enum class ImagePhase { Idle, Loading, Downloaded, Ready, Failed };
    enum class FocusZone { Screenshots, Summary, Actions };
    // Gallery/Active artwork/Restore default/Manage mods stay fixed at 0-3.
    // What follows depends on port state, so draw, activate and nav-clamp all
    // read the same list instead of three separately hand-kept index maps.
    std::vector<std::string> actionLabels() const;
    struct ImageState {
        std::mutex mutex;
        ImagePhase phase = ImagePhase::Idle;
        std::vector<std::uint8_t> bytes;
        std::future<void> future;
        nxui::Texture texture;
        int maxSide = 1024;
    };

    void clearImages();
    void queueImage(const std::string& url, int maxSide);
    void syncImageUploads();
    const nxui::Texture* imageTexture(const std::string& url) const;
    ImagePhase imagePhase(const std::string& url) const;
    void syncLocalCover();
    void clampScreenshotSelection();
    void activateAction();
    nxui::Rect heroRect(const nxui::Rect& content) const;
    nxui::Rect thumbnailRect(const nxui::Rect& content, int index, int total) const;
    static nxui::Rect fitTexture(const nxui::Rect& rect, const nxui::Texture& texture);
    static std::string ellipsize(nxui::Font* font, const std::string& text, float maxWidth, float scale);
    static std::vector<std::string> wrapText(nxui::Font* font, const std::string& text,
                                             float maxWidth, float scale, int maxLines);

    nxui::ThreadPool* m_pool = nullptr;
    nxui::GpuDevice* m_gpu = nullptr;
    nxui::Renderer* m_renderer = nullptr;
    GameMetadataClient m_client;
    GameMetadataClient::Snapshot m_snapshot;
    std::uint64_t m_seenRevision = 0;
    std::uint64_t m_titleId = 0;
    std::string m_title;
    std::string m_searchTitle;
    std::string m_metadataPlatform;
    std::string m_displayVersion;
    std::string m_modSummary;
    std::string m_playTime;
    std::vector<std::uint8_t> m_coverBytes;
    nxui::Texture m_coverTexture;
    nxui::Texture* m_liveCover = nullptr;
    bool m_coverUploadPending = false;
    std::unordered_map<std::string, std::shared_ptr<ImageState>> m_images;
    int m_selectedScreenshot = 0;
    int m_selectedAction = 0;
    int m_summaryScrollLine = 0;
    FocusZone m_focusZone = FocusZone::Screenshots;
    bool m_imageExpanded = false;
    bool m_localOnly = false;
    bool m_isGamePort = false;
    ActionCb m_folderActionCb;
    ActionCb m_removeGamePortCb;
    ActionCb m_markAsGamePortCb;
    ActionCb m_editSearchTitleCb;
    std::string m_folderActionLabel;
    ActionCb m_openGalleryCb;
    ActionCb m_showArtworkCb;
    ActionCb m_restoreArtworkCb;
    ActionCb m_manageModsCb;
    ActionCb m_deleteSoftwareCb;
};
