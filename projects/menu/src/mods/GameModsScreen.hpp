#pragma once

#include "GameModManager.hpp"
#include "settings/TabbedOverlayScreen.hpp"

#include <functional>
#include <string>
#include <vector>

// A title-local LayeredFS manager. It exposes individual folders/files rather
// than pretending all content in atmosphere/contents is one opaque "mod".
class GameModsScreen final : public TabbedOverlayScreen {
public:
    GameModsScreen();
    void openForGame(std::uint64_t titleId, std::string title);
    void removeSelected();
    std::string selectedName() const;

    using RemoveRequestCb = std::function<void()>;
    void onRequestRemove(RemoveRequestCb cb) { m_removeRequestCb = std::move(cb); }

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
    std::string currentAccessibilitySummary() const override;

private:
    void reload();
    void toggleSelected();
    std::string kindLabel(mods::Kind kind) const;

    std::uint64_t m_titleId = 0;
    std::string m_title;
    std::vector<mods::Entry> m_entries;
    int m_selected = 0;
    RemoveRequestCb m_removeRequestCb;
};
