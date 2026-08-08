#include <nxui/core/Texture.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>

namespace nxui {

Texture::~Texture() {
    if (m_gpu && m_mem && m_allocSize > 0)
        m_gpu->freeImageMemory(m_allocSize);
}

bool Texture::loadFromPixels(GpuDevice& gpu, Renderer& ren,
                             const uint8_t* rgba, int w, int h)
{
    return loadImageData(gpu, ren, rgba, (uint64_t)w * h * 4, w, h,
                         DkImageFormat_RGBA8_Unorm);
}

// One body for every format, because this is the allocation path that answered
// failure with svcBreak until it was fixed. A second copy of it for compressed
// images would be a second copy of that bug waiting to be reintroduced.
bool Texture::loadImageData(GpuDevice& gpu, Renderer& ren,
                            const uint8_t* data, uint64_t dataSize,
                            int w, int h, uint32_t format)
{
    m_gpu = &gpu;
    int oldSlot = m_slot;
    uint32_t oldAllocSize = m_allocSize;

    m_valid = false;
    m_slot  = -1;
    m_allocSize = 0;
    m_width  = w;
    m_height = h;

    dk::ImageLayout layout;
    dk::ImageLayoutMaker{gpu.device()}
        .setFlags(0)
        .setFormat((DkImageFormat)format)
        .setDimensions(w, h)
        .initialize(layout);

    uint32_t needed = layout.getSize();
    uint32_t alignedNeeded = (needed + kGpuAlign - 1) & ~(kGpuAlign - 1);
    uint32_t alignedOld    = (oldAllocSize + kGpuAlign - 1) & ~(kGpuAlign - 1);

    if (m_mem && alignedOld >= alignedNeeded) {
        // Reuse existing MemBlock — avoids kernel free+alloc round-trip.
        // Keep the original allocSize so the budget stays accurate.
        m_allocSize = oldAllocSize;
    } else {
        // Need a bigger block — free the old one first.
        if (m_mem && oldAllocSize > 0)
            gpu.freeImageMemory(oldAllocSize);

        m_mem = gpu.allocImageMemory(needed);
        if (!m_mem) {
            std::printf("[Texture] allocImageMemory FAILED (%dx%d) — GPU budget exhausted\n", w, h);
            // Two things had to be put right here, and both of them showed up
            // as a crash rather than as a missing texture.
            //
            // The old block is gone: it was destroyed by the assignment above,
            // and freeImageMemory already gave its bytes back. m_image still
            // described it, and m_valid still said true from the load that
            // succeeded before, so the next frame drew a texture whose memory
            // no longer existed -- deko3d answers that with svcBreak, which is
            // the User Break in the crash reports.
            //
            // m_allocSize still held the old size too, so the next attempt
            // would hand those same bytes back a second time. The budget walks
            // downwards from there and stops bounding anything, which is how a
            // console with enough icons reaches real exhaustion.
            m_valid = false;
            m_slot = -1;
            m_allocSize = 0;
            return false;
        }
        m_allocSize = needed;
    }

    m_image.initialize(layout, m_mem, 0);

    if (!gpu.uploadTexture(m_image, data, (uint32_t)dataSize, w, h, dataSize)) {
        std::printf("[Texture] uploadTexture FAILED (%dx%d)\n", w, h);
        // Half a texture is worse than none: it can still be bound and drawn.
        m_valid = false;
        return false;
    }

    dk::ImageView view{m_image};
    if (oldSlot >= 0) {
        // Reuse the same descriptor slot — avoids slot exhaustion
        ren.updateTexture(oldSlot, view);
        m_slot = oldSlot;
    } else {
        m_slot = ren.registerTexture(view);
        if (m_slot < 0) {
            std::printf("[Texture] registerTexture FAILED (%dx%d) — descriptor pool full\n", w, h);
            // Half a texture is worse than none: it can still be bound and drawn.
            m_valid = false;
            return false;
        }
    }
    m_valid = true;
    return true;
}

bool Texture::loadFromPixelsPooled(GpuDevice& gpu, Renderer& ren,
                                    const uint8_t* rgba, int w, int h)
{
    m_gpu = &gpu;
    m_valid = false;
    m_slot  = -1;
    m_width  = w;
    m_height = h;

    dk::ImageLayout layout;
    dk::ImageLayoutMaker{gpu.device()}
        .setFlags(0)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(w, h)
        .initialize(layout);

    auto alloc = gpu.allocImageFromPool(layout.getSize(), layout.getAlignment());
    if (!alloc.valid()) {
        std::printf("[Texture] pool alloc FAILED (%dx%d) — budget exhausted\n", w, h);
        // Half a texture is worse than none: it can still be bound and drawn.
        m_valid = false;
        return false;
    }
    // m_mem stays empty — pool owns the memory.
    m_image.initialize(layout, alloc.block, alloc.offset);

    if (!gpu.uploadTexture(m_image, rgba, w * h * 4, w, h)) {
        std::printf("[Texture] uploadTexture FAILED (%dx%d)\n", w, h);
        // Half a texture is worse than none: it can still be bound and drawn.
        m_valid = false;
        return false;
    }

    dk::ImageView view{m_image};
    m_slot = ren.registerTexture(view);
    if (m_slot < 0) {
        std::printf("[Texture] registerTexture FAILED (%dx%d) — descriptor pool full\n", w, h);
        // Half a texture is worse than none: it can still be bound and drawn.
        m_valid = false;
        return false;
    }
    m_valid = true;
    return true;
}

// A DDS holding BC1 blocks, which the GPU samples without unpacking. Four bytes
// a pixel is what forced theme animations down to 224x126, where a frame
// stretched to the screen looks like blocks; BC1 is half a byte a pixel, so the
// same memory holds 640x360 and the stretch drops from 5.7x to 2x.
//
// Only the layout ffmpeg and the theme tooling produce is accepted: no mipmaps,
// no cubemaps, no other fourCC. Anything else is a file this was not asked to
// read, and guessing at it is how a loader ends up handing deko3d something it
// answers with svcBreak.
bool Texture::loadBc1File(GpuDevice& gpu, Renderer& ren, const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    uint8_t head[128];
    if (std::fread(head, 1, sizeof(head), f) != sizeof(head)) { std::fclose(f); return false; }

    auto u32 = [&](int off) {
        return (uint32_t)head[off] | ((uint32_t)head[off+1] << 8)
             | ((uint32_t)head[off+2] << 16) | ((uint32_t)head[off+3] << 24);
    };
    const uint32_t fourCC = u32(84);
    const int      hh     = (int)u32(12);
    const int      ww     = (int)u32(16);
    if (u32(0) != 0x20534444u /* "DDS " */ || fourCC != 0x31545844u /* "DXT1" */) {
        std::printf("[Texture] not a DXT1 dds: %s\n", path.c_str());
        std::fclose(f);
        return false;
    }
    // BC1 stores a 4x4 block in 8 bytes, so the dimensions have to be whole
    // blocks -- a partial block would leave the GPU reading past the data.
    if (ww <= 0 || hh <= 0 || (ww & 3) || (hh & 3)) {
        std::printf("[Texture] dds %dx%d is not a multiple of 4: %s\n", ww, hh, path.c_str());
        std::fclose(f);
        return false;
    }

    const size_t blocks = (size_t)(ww / 4) * (hh / 4) * 8;
    std::vector<uint8_t> data(blocks);
    const size_t got = std::fread(data.data(), 1, blocks, f);
    std::fclose(f);
    if (got != blocks) {
        std::printf("[Texture] dds truncated: %zu of %zu bytes in %s\n", got, blocks, path.c_str());
        return false;
    }

    return loadImageData(gpu, ren, data.data(), blocks, ww, hh, DkImageFormat_RGB_BC1);
}

bool Texture::loadFromFile(GpuDevice& gpu, Renderer& ren, const std::string& path, int maxSide) {
    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".dds") == 0)
        return loadBc1File(gpu, ren, path);

    int w, h, ch;
    uint8_t* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
    if (!data) {
        std::printf("[Texture] stbi_load FAILED: %s\n", path.c_str());
        // Half a texture is worse than none: it can still be bound and drawn.
        m_valid = false;
        return false;
    }
    if (maxSide > 0 && (w > maxSide || h > maxSide)) {
        float scale = std::min((float)maxSide / w, (float)maxSide / h);
        int dw = std::max(1, (int)(w * scale));
        int dh = std::max(1, (int)(h * scale));
        uint8_t* scaled = (uint8_t*)std::malloc((size_t)dw * dh * 4);
        if (scaled) {
            for (int y = 0; y < dh; ++y) {
                int sy = y * h / dh;
                for (int x = 0; x < dw; ++x) {
                    int sx = x * w / dw;
                    std::memcpy(scaled + ((size_t)y * dw + x) * 4,
                                data   + ((size_t)sy * w + sx) * 4, 4);
                }
            }
            stbi_image_free(data);
            data = scaled;
            w = dw;
            h = dh;
            // after this, data is malloc'd not stbi
        }
    }
    bool ok = loadFromPixels(gpu, ren, data, w, h);
    // stbi_image_free uses free(), and malloc'd data also uses free()
    std::free(data);
    return ok;
}

bool Texture::loadFromMemory(GpuDevice& gpu, Renderer& ren,
                             const uint8_t* data, size_t dataSize, int maxSide)
{
    int w, h, ch;
    uint8_t* pixels = stbi_load_from_memory(data, (int)dataSize, &w, &h, &ch, 4);
    if (!pixels) return false;

    if (maxSide > 0 && (w > maxSide || h > maxSide)) {
        float scale = std::min((float)maxSide / w, (float)maxSide / h);
        int dw = std::max(1, (int)(w * scale));
        int dh = std::max(1, (int)(h * scale));
        uint8_t* scaled = (uint8_t*)std::malloc((size_t)dw * dh * 4);
        if (scaled) {
            for (int y = 0; y < dh; ++y) {
                int sy = y * h / dh;
                for (int x = 0; x < dw; ++x) {
                    int sx = x * w / dw;
                    std::memcpy(scaled + ((size_t)y * dw + x) * 4,
                                pixels + ((size_t)sy * w + sx) * 4, 4);
                }
            }
            stbi_image_free(pixels);
            pixels = scaled;
            w = dw;
            h = dh;
        }
    }

    bool ok = loadFromPixels(gpu, ren, pixels, w, h);
    std::free(pixels);
    return ok;
}

bool Texture::loadFromSurface(GpuDevice& gpu, Renderer& ren,
                              const uint8_t* data, int w, int h, int pitch)
{
    // Convert to tightly packed RGBA if pitch != w*4
    if (pitch == w * 4) {
        return loadFromPixels(gpu, ren, data, w, h);
    }
    std::vector<uint8_t> tight(w * h * 4);
    for (int y = 0; y < h; ++y)
        std::memcpy(tight.data() + y * w * 4, data + y * pitch, w * 4);
    return loadFromPixels(gpu, ren, tight.data(), w, h);
}

} // namespace nxui
