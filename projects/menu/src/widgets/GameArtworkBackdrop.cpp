#include "GameArtworkBackdrop.hpp"

#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>

#include <algorithm>

bool GameArtworkBackdrop::setArtwork(nxui::GpuDevice& gpu, nxui::Renderer& ren,
                                     const std::string& path) {
    if (path == m_path)
        return m_texture.valid();
    if (path.empty()) {
        clearArtwork(&gpu);
        return true;
    }
    nxui::Texture loaded;
    if (!loaded.loadFromFile(gpu, ren, path, 1280))
        return false;
    // The texture being replaced can still be referenced by the frame in
    // flight. Freeing it underneath the GPU is how moving the cursor across a
    // game with custom art took the whole menu down.
    if (m_texture.valid())
        gpu.waitIdle();
    m_texture = std::move(loaded);
    m_path = path;
    return true;
}

void GameArtworkBackdrop::clearArtwork(nxui::GpuDevice* gpu) {
    if (gpu && m_texture.valid())
        gpu->waitIdle();
    m_texture = nxui::Texture{};
    m_path.clear();
}

void GameArtworkBackdrop::onRender(nxui::Renderer& ren) {
    if (!m_texture.valid() || opacity() <= 0.f)
        return;
    const nxui::Rect dest = rect();
    const float sourceAspect = static_cast<float>(m_texture.width()) / m_texture.height();
    const float destAspect = dest.width / dest.height;
    nxui::Rect source{0.f, 0.f, static_cast<float>(m_texture.width()), static_cast<float>(m_texture.height())};
    if (sourceAspect > destAspect) {
        source.width = source.height * destAspect;
        source.x = (m_texture.width() - source.width) * 0.5f;
    } else {
        source.height = source.width / destAspect;
        source.y = (m_texture.height() - source.height) * 0.5f;
    }
    ren.drawTextureSub(&m_texture, source, dest, nxui::Color::white().withAlpha(0.46f * opacity()));
    ren.drawGradientRect(dest, nxui::Color(0.01f, 0.02f, 0.04f, 0.20f * opacity()),
                         nxui::Color(0.01f, 0.02f, 0.04f, 0.62f * opacity()));
}
