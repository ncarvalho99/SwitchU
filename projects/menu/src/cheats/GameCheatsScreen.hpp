#pragma once

#include "GameCheatManager.hpp"
#include "settings/TabbedOverlayScreen.hpp"

#include <functional>
#include <string>
#include <vector>

class GameCheatsScreen final : public TabbedOverlayScreen {
public:
    GameCheatsScreen();
    void openForGame(std::uint64_t titleId, std::string title);
    int buildCount() const { return static_cast<int>(m_builds.size()); }

    using ToggleOffCb = std::function<void()>;
    void onToggleOffSfx(ToggleOffCb cb) { m_toggleOffSfxCb = std::move(cb); }

protected:
    void buildTabs() override;
    bool usesCustomContentLayout() const override { return true; }
    bool customContentUsesPanel() const override { return true; }
    bool drawsCustomContentPanel() const override { return false; }
    bool hidesTabRail() const override { return true; }
    void drawCustomContent(nxui::Renderer& ren, const nxui::Rect& panel,
                           const nxui::Rect& content, float opacity) override;
    bool handleCustomPressA() override;
    bool handleCustomPressB() override;
    bool handleCustomPressX() override;
    bool handleCustomNavUp() override;
    bool handleCustomNavDown() override;
    bool handleCustomNavLeft() override;
    bool handleCustomNavRight() override;
    std::string currentAccessibilitySummary() const override;

private:
    void reload();
    void toggleCurrent();
    void toggleAll();
    void switchBuild(int delta);
    const cheats::BuildCheats* currentBuildCheats() const;
    cheats::BuildCheats* currentBuildCheats();
    int currentCheatsCount() const;

    std::uint64_t m_titleId = 0;
    std::string m_title;
    std::vector<cheats::BuildCheats> m_builds;
    int m_currentBuild = 0;
    int m_selected = 0;
    ToggleOffCb m_toggleOffSfxCb;
};
