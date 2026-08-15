#pragma once

#include <nxui/core/Texture.hpp>
#include <nxui/widgets/Widget.hpp>

#include <string>

namespace nxui {
class GpuDevice;
class Renderer;
}

// A selected game's hero image. It stays below all home controls, so a custom
// background feels like artwork for that software rather than a new theme.
class GameArtworkBackdrop final : public nxui::Widget {
public:
    bool setArtwork(nxui::GpuDevice& gpu, nxui::Renderer& ren, const std::string& path);
    // Takes the device because releasing the outgoing texture has to wait for
    // the frame still sampling it. Focus moves call this on every step across
    // the grid, so the release cannot be left to chance.
    void clearArtwork(nxui::GpuDevice* gpu = nullptr);
    const std::string& artworkPath() const { return m_path; }

protected:
    void onRender(nxui::Renderer& ren) override;

private:
    nxui::Texture m_texture;
    std::string m_path;
};
