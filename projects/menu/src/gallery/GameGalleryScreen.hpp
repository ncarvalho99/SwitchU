#pragma once

#include "GameGalleryClient.hpp"
#include "settings/TabbedOverlayScreen.hpp"

#include <nxui/core/Texture.hpp>

#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace nxui {
class GpuDevice;
class Renderer;
class ThreadPool;
}

class GameGalleryScreen final : public TabbedOverlayScreen {
public:
    GameGalleryScreen();

    void setThreadPool(nxui::ThreadPool* pool) { m_pool = pool; }
    void setRenderContext(nxui::GpuDevice* gpu, nxui::Renderer* renderer) {
        m_gpu = gpu;
        m_renderer = renderer;
    }
    void openForGame(std::uint64_t titleId, std::string title);
    bool isFullscreen() const { return m_fullscreen; }

    using ApplyArtworkCb = std::function<void(std::uint64_t, const std::string&,
                                              GameGalleryClient::Category,
                                              GameGalleryClient::Asset,
                                              std::shared_ptr<const std::vector<std::uint8_t>>)>;
    void onApplyArtwork(ApplyArtworkCb cb) { m_applyArtworkCb = std::move(cb); }

    using ResetArtworkCb = std::function<void(std::uint64_t, const std::string&,
                                              GameGalleryClient::Category)>;
    void onResetArtwork(ResetArtworkCb cb) { m_resetArtworkCb = std::move(cb); }

protected:
    void buildTabs() override;
    bool usesCustomContentLayout() const override { return true; }
    void drawCustomContent(nxui::Renderer& ren, const nxui::Rect& panel,
                           const nxui::Rect& content, float opacity) override;
    void updateCustomContent(float dt) override;
    bool handleCustomPressA() override;
    bool handleCustomPressB() override;
    bool handleCustomPressX() override;
    bool handleCustomNavUp() override;
    bool handleCustomNavDown() override;
    bool handleCustomNavLeft() override;
    bool handleCustomNavRight() override;
    bool handleCustomTouch(nxui::Input& input, const nxui::Rect& panel,
                           const nxui::Rect& tabs, const nxui::Rect& content) override;
    std::string currentAccessibilitySummary() const override;

private:
    enum class PreviewPhase { Idle, Loading, Downloaded, Ready, Failed };
    struct PreviewState {
        std::mutex mutex;
        PreviewPhase phase = PreviewPhase::Idle;
        std::vector<std::uint8_t> bytes;
        std::future<void> future;
        nxui::Texture texture;
        int maxSide = 512;
    };

    static constexpr int kColumns = 3;
    static constexpr int kRows = 2;
    static constexpr int kPerPage = kColumns * kRows;

    void refreshCategory();
    void queuePreview(const std::string& url, int maxSide);
    void syncPreviewUploads();
    void clearPreviews();
    void requestApplySelected();
    void requestResetArtwork();
    void cycleDimensions();
    std::string currentDimensions() const;
    const nxui::Texture* previewTexture(const std::string& url) const;
    PreviewPhase previewPhase(const std::string& url) const;
    int visibleStart() const;
    int visibleEnd() const;
    void clampSelection();
    nxui::Rect cardRect(const nxui::Rect& content, int localIndex) const;
    static nxui::Rect fitTexture(const nxui::Rect& rect, const nxui::Texture& texture);

    nxui::ThreadPool* m_pool = nullptr;
    nxui::GpuDevice* m_gpu = nullptr;
    nxui::Renderer* m_renderer = nullptr;
    GameGalleryClient m_client;
    GameGalleryClient::Snapshot m_snapshot;
    std::unordered_map<std::string, std::shared_ptr<PreviewState>> m_previews;
    std::string m_title;
    std::uint64_t m_titleId = 0;
    std::uint64_t m_seenRevision = 0;
    int m_seenTab = -1;
    int m_selected = 0;
    int m_page = 0;
    int m_touchAsset = -1;
    int m_gridDimensionIndex = 0;
    int m_heroDimensionIndex = 0;
    bool m_fullscreen = false;
    ApplyArtworkCb m_applyArtworkCb;
    ResetArtworkCb m_resetArtworkCb;
};
