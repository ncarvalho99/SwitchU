#pragma once

#include "ActivityLogManager.hpp"
#include "settings/TabbedOverlayScreen.hpp"

#include <nxui/core/Texture.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace nxui {
class GpuDevice;
class Renderer;
class ThreadPool;
}

class ActivityLogScreen final : public TabbedOverlayScreen {
public:
    ActivityLogScreen();
    ~ActivityLogScreen() override;

    void setThreadPool(nxui::ThreadPool* pool) { m_pool = pool; }
    void setRenderContext(nxui::GpuDevice* gpu, nxui::Renderer* renderer) {
        m_gpu = gpu;
        m_renderer = renderer;
    }
    void setManager(switchu::activity::ActivityLogManager* mgr) { m_manager = mgr; }

    void open(std::uint64_t initialTitleId = 0);

    using VoidCb = std::function<void()>;
    using BoolCb = std::function<void(bool)>;
    using TitleCb = std::function<void(std::uint64_t)>;
    using IconProvider = std::function<nxui::Texture*(std::uint64_t)>;

    void setInput(nxui::Input* input)            { m_input = input; }
    void setIconProvider(IconProvider provider)  { m_iconProvider = std::move(provider); }
    void onClose(VoidCb cb)                      { m_closeCb = std::move(cb); }
    void onTabChangeSfx(VoidCb cb)               { m_tabChangeSfxCb = std::move(cb); }
    void onDateChangeSfx(BoolCb cb)              { m_dateChangeSfxCb = std::move(cb); }
    void onTitleSelected(TitleCb cb)             { m_titleSelectedCb = std::move(cb); }

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
                           const nxui::Rect& tr, const nxui::Rect& content) override;

    std::string currentAccessibilitySummary() const override;

private:
    void cycleTab(int delta);
    void cycleDay(int delta);
    void cycleMonth(int delta);
    void jumpToToday();

    void drawDailyTab(nxui::Renderer& ren, const nxui::Rect& content, float opacity);
    void drawMonthlyTab(nxui::Renderer& ren, const nxui::Rect& content, float opacity);
    void drawTitlesTab(nxui::Renderer& ren, const nxui::Rect& content, float opacity);

    std::string formatPlaytime(std::uint64_t seconds) const;
    std::string formatAverageSession(std::uint64_t seconds) const;
    std::string formatDateHeading(int y, int m, int d) const;
    std::string formatMonthHeading(int y, int m) const;
    std::string formatDateNumeric(std::uint64_t posixSeconds) const;

    nxui::Color getTitleColor(size_t index) const;

    nxui::ThreadPool* m_pool = nullptr;
    nxui::GpuDevice* m_gpu = nullptr;
    nxui::Renderer* m_renderer = nullptr;
    switchu::activity::ActivityLogManager* m_manager = nullptr;

    VoidCb m_closeCb;
    VoidCb m_tabChangeSfxCb;
    BoolCb m_dateChangeSfxCb;
    TitleCb m_titleSelectedCb;

    // Today's anchor
    int m_todayYear = 2026;
    int m_todayMonth = 9;
    int m_todayDay = 10;

    // Daily Tab State
    int m_curYear = 2026;
    int m_curMonth = 9;
    int m_curDay = 10;
    int m_dailyScrollIndex = 0;
    int m_dailyFocus = 0; // 0 = Date header, 1 = Titles list

    // Monthly Tab State
    int m_monthYear = 2026;
    int m_monthMonth = 9;
    int m_monthlySelectedDay = 10; // day 1..31
    int m_monthlyFocus = 0; // 0 = Month header, 1 = Calendar graph, 2 = Titles list

    // Software Library Tab State
    int m_selectedTitleIdx = 0;
    int m_titleScrollTop = 0;

    // Touch interaction
    int m_touchDownTarget = -1; // -1: none, 0: prev, 1: next, 2: today, 10+: list item

    nxui::Input* m_input = nullptr;
    IconProvider m_iconProvider;
    std::unordered_map<std::uint64_t, std::unique_ptr<nxui::Texture>> m_cachedIcons;

    nxui::Texture* getIconTexture(std::uint64_t titleId);
    void setupCustomKeyActions();
};
