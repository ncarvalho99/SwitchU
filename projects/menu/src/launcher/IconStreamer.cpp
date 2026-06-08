#include "IconStreamer.hpp"
#include "widgets/GlossyIcon.hpp"
#include "core/DebugLog.hpp"
#include <nxui/third_party/stb/stb_image.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>


void IconStreamer::init(int appCount) {
    clear();
    m_compressed.resize(appCount);
    m_titleIds.assign(appCount, 0);
    m_appToSlot.assign(appCount, -1);
}

void IconStreamer::setIconDataLoader(IconDataLoader loader) {
    m_iconLoader = std::move(loader);
}

void IconStreamer::setTitleId(int appIndex, uint64_t titleId) {
    if (appIndex >= 0 && appIndex < (int)m_titleIds.size())
        m_titleIds[appIndex] = titleId;
}

void IconStreamer::setIconData(int appIndex, std::vector<uint8_t> compressed) {
    if (appIndex >= 0 && appIndex < (int)m_compressed.size())
        m_compressed[appIndex] = std::move(compressed);
}

void IconStreamer::resize(int appCount) {
    if (appCount < 0)
        appCount = 0;

    for (auto& slot : m_pool) {
        if (slot && slot->appIndex >= appCount)
            slot->appIndex = -1;
    }

    m_compressed.resize(appCount);
    m_titleIds.resize(appCount, 0);
    m_appToSlot.resize(appCount, -1);
    if (m_pinnedIndex >= appCount)
        m_pinnedIndex = -1;
    m_lastPage = -1;
    m_lastIconsPerPage = -1;
}

void IconStreamer::setPinnedIndex(int appIndex) {
    m_pinnedIndex = (appIndex >= 0 && appIndex < (int)m_appToSlot.size()) ? appIndex : -1;
}

void IconStreamer::clearPinnedIndex() {
    m_pinnedIndex = -1;
}

void IconStreamer::clear() {
    m_pool.clear();
    m_compressed.clear();
    m_titleIds.clear();
    m_appToSlot.clear();
    m_freeSlots.clear();
    m_lastPage = -1;
    m_lastIconsPerPage = -1;
    m_pinnedIndex = -1;
}

bool IconStreamer::swapIndices(int a, int b) {
    if (a < 0 || b < 0 || a >= (int)m_appToSlot.size() || b >= (int)m_appToSlot.size())
        return false;
    if (a == b)
        return true;

    if (a < (int)m_compressed.size() && b < (int)m_compressed.size())
        std::swap(m_compressed[a], m_compressed[b]);
    if (a < (int)m_titleIds.size() && b < (int)m_titleIds.size())
        std::swap(m_titleIds[a], m_titleIds[b]);

    if (a < (int)m_appToSlot.size() && b < (int)m_appToSlot.size()) {
        int slotA = m_appToSlot[a];
        int slotB = m_appToSlot[b];
        std::swap(m_appToSlot[a], m_appToSlot[b]);

        if (slotA >= 0 && slotA < (int)m_pool.size())
            m_pool[slotA]->appIndex = b;
        if (slotB >= 0 && slotB < (int)m_pool.size())
            m_pool[slotB]->appIndex = a;
    }

    if (m_pinnedIndex == a)
        m_pinnedIndex = b;
    else if (m_pinnedIndex == b)
        m_pinnedIndex = a;

    m_lastPage = -1;
    m_lastIconsPerPage = -1;
    return true;
}

bool IconStreamer::hasData(int index) const {
    if (index < 0 || index >= (int)m_appToSlot.size())
        return false;
    if (index < (int)m_compressed.size() && !m_compressed[index].empty())
        return true;
    return index < (int)m_titleIds.size() && m_titleIds[index] != 0 && (bool)m_iconLoader;
}

// ---------------------------------------------------------------------------
// Decode a single compressed icon to RGBA, downscaling to kIconSize if needed.
// ---------------------------------------------------------------------------
IconStreamer::DecodedIcon IconStreamer::decodeAndScale(const std::vector<uint8_t>& data) const {
    DecodedIcon out{};
    if (data.empty()) return out;

    int w, h, ch;
    uint8_t* full = stbi_load_from_memory(data.data(), (int)data.size(),
                                           &w, &h, &ch, 4);
    if (!full) return out;

    if (w > kIconSize || h > kIconSize) {
        int dstW = kIconSize, dstH = kIconSize;
        uint8_t* scaled = (uint8_t*)std::malloc((size_t)dstW * dstH * 4);
        if (scaled) {
            float scaleX = (float)w / dstW;
            float scaleY = (float)h / dstH;
            for (int y = 0; y < dstH; ++y) {
                float srcYf = (y + 0.5f) * scaleY - 0.5f;
                int y0 = (int)srcYf; if (y0 < 0) y0 = 0;
                int y1 = y0 + 1;     if (y1 >= h) y1 = h - 1;
                float fy = srcYf - y0;
                for (int x = 0; x < dstW; ++x) {
                    float srcXf = (x + 0.5f) * scaleX - 0.5f;
                    int x0 = (int)srcXf; if (x0 < 0) x0 = 0;
                    int x1 = x0 + 1;     if (x1 >= w) x1 = w - 1;
                    float fx = srcXf - x0;
                    const uint8_t* p00 = full + ((size_t)y0 * w + x0) * 4;
                    const uint8_t* p10 = full + ((size_t)y0 * w + x1) * 4;
                    const uint8_t* p01 = full + ((size_t)y1 * w + x0) * 4;
                    const uint8_t* p11 = full + ((size_t)y1 * w + x1) * 4;
                    uint8_t* dst = scaled + ((size_t)y * dstW + x) * 4;
                    for (int c = 0; c < 4; ++c) {
                        dst[c] = (uint8_t)(
                            p00[c] * (1 - fx) * (1 - fy) +
                            p10[c] * fx       * (1 - fy) +
                            p01[c] * (1 - fx) * fy       +
                            p11[c] * fx       * fy       + 0.5f);
                    }
                }
            }
            stbi_image_free(full);
            out.rgba = scaled;
            out.w = dstW;
            out.h = dstH;
            out.scaledWithMalloc = true;
        } else {
            out.rgba = full;
            out.w = w;
            out.h = h;
        }
    } else {
        out.rgba = full;
        out.w = w;
        out.h = h;
    }
    return out;
}

// ---------------------------------------------------------------------------
// Core streaming logic.
// ---------------------------------------------------------------------------
void IconStreamer::onPageChanged(int currentPage, int iconsPerPage,
                                 nxui::GpuDevice& gpu, nxui::Renderer& ren,
                                 const std::vector<std::shared_ptr<GlossyIcon>>& allIcons)
{
    int totalApps  = (int)m_appToSlot.size();
    if (totalApps == 0 || iconsPerPage <= 0) {
        m_lastPage = currentPage;
        m_lastIconsPerPage = iconsPerPage;
        return;
    }

    int totalPages = (totalApps + iconsPerPage - 1) / iconsPerPage;
    currentPage = std::clamp(currentPage, 0, totalPages - 1);

    m_lastPage = currentPage;
    m_lastIconsPerPage = iconsPerPage;

    int visibleStartApp = currentPage * iconsPerPage;
    int visibleEndApp   = std::min(totalApps, visibleStartApp + iconsPerPage);
    int cacheStartPage  = std::max(0, currentPage - kPageCacheRadius);
    int cacheEndPage    = std::min(totalPages - 1, currentPage + kPageCacheRadius);
    int cacheStartApp   = cacheStartPage * iconsPerPage;
    int cacheEndApp     = std::min(totalApps, (cacheEndPage + 1) * iconsPerPage);

    // 1. Evict textures outside the local page window. This keeps GPU memory
    //    bounded while preserving quick navigation to nearby pages.
    for (int i = 0; i < (int)m_pool.size(); ++i) {
        if (!m_pool[i])
            continue;

        int app = m_pool[i]->appIndex;
        if (app < 0 || app == m_pinnedIndex)
            continue;

        if (app < cacheStartApp || app >= cacheEndApp) {
            if (app < (int)allIcons.size())
                allIcons[app]->setTexture(nullptr);
            if (app < (int)m_appToSlot.size())
                m_appToSlot[app] = -1;
            m_pool[i]->appIndex = -1;
            m_freeSlots.push_back(i);
        }
    }

    // 2. Re-attach already-loaded slots to the current widget order. This is
    //    needed after grid relayouts and swaps where the GlossyIcon objects
    //    may have moved while the GPU texture pool stayed valid.
    for (int i = visibleStartApp; i < visibleEndApp; ++i) {
        int slotIdx = m_appToSlot[i];
        if (slotIdx < 0)
            continue;

        bool validSlot = slotIdx < (int)m_pool.size() &&
                         m_pool[slotIdx] &&
                         m_pool[slotIdx]->appIndex == i &&
                         m_pool[slotIdx]->texture.valid();
        if (validSlot) {
            if (i < (int)allIcons.size())
                allIcons[i]->setTexture(&m_pool[slotIdx]->texture);
        } else {
            m_appToSlot[i] = -1;
            if (i < (int)allIcons.size())
                allIcons[i]->setTexture(nullptr);
        }
    }

    // 3. Collect only visible apps that need loading. Neighbor pages are kept
    //    when already loaded, but not decoded eagerly on this frame.
    std::vector<int> toLoad;
    for (int i = visibleStartApp; i < visibleEndApp; ++i) {
        if (m_appToSlot[i] < 0 && hasData(i))
            toLoad.push_back(i);
    }

    if (toLoad.empty()) return;

    struct PendingIcon {
        int appIndex;
        std::vector<uint8_t> compressed;
    };
    std::vector<PendingIcon> pending;
    pending.reserve(toLoad.size());

    for (int appIndex : toLoad) {
        std::vector<uint8_t> compressed;
        if (appIndex < (int)m_compressed.size() && !m_compressed[appIndex].empty()) {
            compressed = m_compressed[appIndex];
        } else if (appIndex < (int)m_titleIds.size() && m_titleIds[appIndex] != 0 && m_iconLoader) {
            compressed = m_iconLoader(m_titleIds[appIndex]);
        }

        if (!compressed.empty())
            pending.push_back({appIndex, std::move(compressed)});
    }

    if (pending.empty()) return;

    DebugLog::log("[streamer] page %d: loading %d icons [%d..%d)",
                  currentPage, (int)pending.size(), visibleStartApp, visibleEndApp);

    // 4. Decode icons.
    struct Decoded {
        int appIndex;
        uint8_t* rgba = nullptr;
        int w = 0, h = 0;
        bool scaledWithMalloc = false;
    };
    std::vector<Decoded> decoded(pending.size());
    for (int idx = 0; idx < (int)pending.size(); ++idx) {
        auto result = decodeAndScale(pending[idx].compressed);
        decoded[idx] = {pending[idx].appIndex, result.rgba, result.w, result.h, result.scaledWithMalloc};
    }
    pending.clear();

    // 5. Upload to GPU (must happen on the main/render thread) and
    //    wire the texture pointers on the corresponding GlossyIcons.

    // Pre-reserve pool capacity so emplace_back() never reallocates.
    // Reallocation would invalidate texture pointers already handed out
    // to GlossyIcon widgets earlier in this loop.
    {
        int newSlots = 0;
        int freeAvail = (int)m_freeSlots.size();
        for (auto& d : decoded) {
            if (!d.rgba) continue;
            if (freeAvail > 0) --freeAvail;
            else ++newSlots;
        }
        m_pool.reserve(m_pool.size() + newSlots);
    }

    for (auto& d : decoded) {
        if (!d.rgba) continue;

        // Acquire a pool slot.
        int poolIdx;
        if (!m_freeSlots.empty()) {
            poolIdx = m_freeSlots.back();
            m_freeSlots.pop_back();
        } else {
            poolIdx = (int)m_pool.size();
            m_pool.emplace_back(std::make_unique<TexSlot>());
        }

        auto& slot = *m_pool[poolIdx];
        if (slot.texture.loadFromPixels(gpu, ren, d.rgba, d.w, d.h)) {
            slot.appIndex = d.appIndex;
            m_appToSlot[d.appIndex] = poolIdx;
            if (d.appIndex < (int)allIcons.size())
                allIcons[d.appIndex]->setTexture(&slot.texture);
        }

        if (d.scaledWithMalloc) std::free(d.rgba);
        else stbi_image_free(d.rgba);
    }
}

void IconStreamer::forceReload(int currentPage, int iconsPerPage,
                                nxui::GpuDevice& gpu, nxui::Renderer& ren,
                                const std::vector<std::shared_ptr<GlossyIcon>>& allIcons)
{
    // Throw away all loaded state so onPageChanged re-does everything.
    for (auto& slot : m_pool) slot->appIndex = -1;
    m_freeSlots.clear();
    for (int i = 0; i < (int)m_pool.size(); ++i) m_freeSlots.push_back(i);
    std::fill(m_appToSlot.begin(), m_appToSlot.end(), -1);

    // Clear the Texture objects themselves (GPU memory + descriptor slots)
    // because a forceReload typically follows a full GPU reset.
    m_pool.clear();
    m_freeSlots.clear();

    m_lastPage = -1;
    m_lastIconsPerPage = -1;
    onPageChanged(currentPage, iconsPerPage, gpu, ren, allIcons);
}
