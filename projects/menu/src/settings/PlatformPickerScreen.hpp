#pragma once

#include <nxui/Theme.hpp>
#include <nxui/core/Font.hpp>
#include <nxui/core/Input.hpp>
#include <nxui/core/ThreadPool.hpp>
#include <nxui/core/Texture.hpp>
#include <nxui/widgets/GlassWidget.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class PlatformPickerScreen final : public nxui::GlassWidget {
public:
    enum class Availability { Checking, Available, Unavailable };

    PlatformPickerScreen(nxui::GpuDevice& gpu, nxui::Renderer& renderer,
                         nxui::ThreadPool& threadPool);

    void setFont(nxui::Font* font) { m_font = font; }
    void setSmallFont(nxui::Font* font) { m_smallFont = font; }
    void setAssetBase(std::string assetBase) { m_assetBase = std::move(assetBase); }
    void setTheme(const nxui::Theme* theme) { m_theme = theme; }
    void showForTitle(std::string title);
    void hide();
    void wait();
    bool isActive() const { return m_active.load(std::memory_order_acquire); }
    void handleTouch(nxui::Input& input);
    void onSelected(std::function<void(const std::string&)> cb) { m_selectedCb = std::move(cb); }
    void onClosed(std::function<void()> cb) { m_closedCb = std::move(cb); }
    // Fired instead of onSelected when A/touch lands on a card whose metadata
    // availability came back Unavailable: there is nothing an online lookup
    // could return for that platform, so committing to it just leaves the
    // dossier permanently empty.
    void onRejected(std::function<void()> cb) { m_rejectedCb = std::move(cb); }

protected:
    void onContentUpdate(float dt) override;
    void onContentRender(nxui::Renderer& renderer) override;

private:
    struct Platform {
        std::string label;
        std::string slug;
        std::string icon;
        Availability availability = Availability::Checking;
        nxui::Texture texture;
    };

    void moveSelection(int dx, int dy);
    void selectCurrent();
    static nxui::Rect containRect(const nxui::Texture& texture, const nxui::Rect& area);

    nxui::GpuDevice& m_gpu;
    nxui::Renderer& m_renderer;
    nxui::ThreadPool& m_threadPool;
    nxui::Font* m_font = nullptr;
    nxui::Font* m_smallFont = nullptr;
    const nxui::Theme* m_theme = nullptr;
    std::array<Platform, 12> m_platforms;
    std::array<nxui::Rect, 12> m_cardRects{};
    std::array<std::future<void>, 12> m_availabilityFutures;
    std::array<std::shared_ptr<std::atomic<Availability>>, 12> m_availabilityResults;
    std::vector<std::future<void>> m_retiredAvailabilityFutures;
    std::unordered_map<std::string, std::array<Availability, 12>> m_availabilityCache;
    std::string m_title;
    std::string m_assetBase;
    std::function<void(const std::string&)> m_selectedCb;
    std::function<void()> m_closedCb;
    std::function<void()> m_rejectedCb;
    std::atomic_uint64_t m_generation = 0;
    int m_selected = 0;
    std::atomic_bool m_active = false;
    float m_spinner = 0.f;
    bool m_backdropCacheValid = false;
    float m_cachedPreBlurRadius = -1.f;
    int m_cachedBlurIterations = -1;
};
