#include "IconStreamer.hpp"
#include "widgets/GlossyIcon.hpp"
#include "core/DebugLog.hpp"
#include <nxui/third_party/stb/stb_image.h>
#include <switch.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <future>
#include <utility>


void IconStreamer::init(int appCount) {
    clear();
    m_compressed.resize(appCount);
    m_titleIds.assign(appCount, 0);
    m_appToSlot.assign(appCount, -1);
    m_customArtwork.assign(appCount, false);
}

void IconStreamer::setIconDataLoader(IconDataLoader loader) {
    m_iconLoader = std::move(loader);
}

void IconStreamer::setArtworkDataLoader(IconDataLoader loader) {
    m_artworkLoader = std::move(loader);
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
    m_customArtwork.resize(appCount, false);
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
    m_customArtwork.clear();
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
    if (a < (int)m_customArtwork.size() && b < (int)m_customArtwork.size()) {
        const bool customA = m_customArtwork[a];
        m_customArtwork[a] = m_customArtwork[b];
        m_customArtwork[b] = customA;
    }

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
IconStreamer::DecodedIcon IconStreamer::decodeAndScale(const std::vector<uint8_t>& data,
                                                        bool customArtwork) const {
    DecodedIcon out{};
    if (data.empty()) return out;

    int w, h, ch;
    uint8_t* full = stbi_load_from_memory(data.data(), (int)data.size(),
                                           &w, &h, &ch, 4);
    if (!full) return out;

    // A square SteamGridDB grid already matches the Switch home icon. It uses
    // the normal fast path below; only a genuinely non-square cover needs the
    // poster composition.
    if (customArtwork && std::abs(w - h) > 1) {
        // SteamGridDB covers are portrait, but the Switch home grid owns a
        // square texture budget.  Do not squash the source into that square:
        // build a compact "poster over blurred art" composition instead.  The
        // sharp foreground keeps the game title intact while its dark blurred
        // copy fills the sides without an empty letterbox.
        constexpr int side = kIconSize;
        uint8_t* composed = (uint8_t*)std::malloc((size_t)side * side * 4);
        if (!composed) {
            out.rgba = full;
            out.w = w;
            out.h = h;
            return out;
        }

        auto sample = [full, w, h](float sourceX, float sourceY, uint8_t* dst) {
            sourceX = std::clamp(sourceX, 0.f, (float)(w - 1));
            sourceY = std::clamp(sourceY, 0.f, (float)(h - 1));
            const int x0 = (int)sourceX, y0 = (int)sourceY;
            const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
            const float fx = sourceX - x0, fy = sourceY - y0;
            const uint8_t* p00 = full + ((size_t)y0 * w + x0) * 4;
            const uint8_t* p10 = full + ((size_t)y0 * w + x1) * 4;
            const uint8_t* p01 = full + ((size_t)y1 * w + x0) * 4;
            const uint8_t* p11 = full + ((size_t)y1 * w + x1) * 4;
            for (int c = 0; c < 4; ++c)
                dst[c] = (uint8_t)(p00[c] * (1 - fx) * (1 - fy) +
                                   p10[c] * fx       * (1 - fy) +
                                   p01[c] * (1 - fx) * fy       +
                                   p11[c] * fx       * fy       + 0.5f);
        };

        // Start with a centred crop only for the backdrop, then blur and dim
        // it. The uncut source is drawn sharply below.
        const float fillScale = std::max((float)side / w, (float)side / h);
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                uint8_t* dst = composed + ((size_t)y * side + x) * 4;
                sample((x + 0.5f) / fillScale - 0.5f + (w - side / fillScale) * 0.5f,
                       (y + 0.5f) / fillScale - 0.5f + (h - side / fillScale) * 0.5f, dst);
                dst[3] = 255;
            }
        }
        std::vector<uint8_t> blurred((size_t)side * side * 4);
        constexpr int blurRadius = 5;
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                int sums[3] = {};
                int count = 0;
                for (int offset = -blurRadius; offset <= blurRadius; ++offset) {
                    const int sx = std::clamp(x + offset, 0, side - 1);
                    const uint8_t* pixel = composed + ((size_t)y * side + sx) * 4;
                    for (int c = 0; c < 3; ++c) sums[c] += pixel[c];
                    ++count;
                }
                uint8_t* dst = blurred.data() + ((size_t)y * side + x) * 4;
                for (int c = 0; c < 3; ++c) dst[c] = (uint8_t)(sums[c] / count);
                dst[3] = 255;
            }
        }
        for (int y = 0; y < side; ++y) {
            for (int x = 0; x < side; ++x) {
                int sums[3] = {};
                int count = 0;
                for (int offset = -blurRadius; offset <= blurRadius; ++offset) {
                    const int sy = std::clamp(y + offset, 0, side - 1);
                    const uint8_t* pixel = blurred.data() + ((size_t)sy * side + x) * 4;
                    for (int c = 0; c < 3; ++c) sums[c] += pixel[c];
                    ++count;
                }
                uint8_t* dst = composed + ((size_t)y * side + x) * 4;
                for (int c = 0; c < 3; ++c) dst[c] = (uint8_t)((sums[c] / count) * 0.32f);
                dst[3] = 255;
            }
        }

        const float posterScale = std::min((float)side / w, (float)side / h);
        const int posterW = std::max(1, (int)std::lround(w * posterScale));
        const int posterH = std::max(1, (int)std::lround(h * posterScale));
        const int posterX = (side - posterW) / 2, posterY = (side - posterH) / 2;
        for (int y = 0; y < posterH; ++y) {
            for (int x = 0; x < posterW; ++x) {
                uint8_t* dst = composed + ((size_t)(posterY + y) * side + posterX + x) * 4;
                sample((x + 0.5f) / posterScale - 0.5f,
                       (y + 0.5f) / posterScale - 0.5f, dst);
            }
        }
        stbi_image_free(full);
        out.rgba = composed;
        out.w = side;
        out.h = side;
        out.scaledWithMalloc = true;
        return out;
    }

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
            if (app < (int)allIcons.size())
                allIcons[app]->setCustomArtwork(false);
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
            if (i < (int)allIcons.size())
                allIcons[i]->setCustomArtwork(i < (int)m_customArtwork.size() && m_customArtwork[i]);
        } else {
            m_appToSlot[i] = -1;
            if (i < (int)allIcons.size())
                allIcons[i]->setTexture(nullptr);
            if (i < (int)allIcons.size())
                allIcons[i]->setCustomArtwork(false);
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

    const uint64_t tickFetchStart = armGetSystemTick();

    struct PendingIcon {
        int appIndex = -1;
        std::vector<uint8_t> compressed;
        DecodedIcon decoded{};
        bool customArtwork = false;
    };
    // Fixed size up front: the decode jobs below hold pointers into this.
    std::vector<PendingIcon> pending(toLoad.size());
    size_t pendingCount = 0;

    // Read the compressed bytes on this thread. The menu applet runs with
    // __nx_fs_num_sessions = 1, so concurrent SD reads would just serialize
    // on the single fs session anyway. Each decode is handed to the pool the
    // moment its bytes land, so decoding overlaps the following reads.
    std::vector<std::future<void>> decodeJobs;
    if (m_threadPool)
        decodeJobs.reserve(toLoad.size());

    for (int appIndex : toLoad) {
        std::vector<uint8_t> compressed;
        bool customArtwork = false;
        if (appIndex < (int)m_titleIds.size() && m_titleIds[appIndex] != 0 && m_artworkLoader) {
            compressed = m_artworkLoader(m_titleIds[appIndex]);
            customArtwork = !compressed.empty();
        }
        if (compressed.empty() && appIndex < (int)m_compressed.size() && !m_compressed[appIndex].empty()) {
            compressed = m_compressed[appIndex];
        } else if (compressed.empty() && appIndex < (int)m_titleIds.size() && m_titleIds[appIndex] != 0 && m_iconLoader) {
            compressed = m_iconLoader(m_titleIds[appIndex]);
        }

        if (compressed.empty())
            continue;

        PendingIcon* job = &pending[pendingCount++];
        job->appIndex = appIndex;
        job->compressed = std::move(compressed);
        job->customArtwork = customArtwork;

        if (m_threadPool) {
            // decodeAndScale is const and touches nothing shared, and each job
            // owns its own bytes and output buffer.
            decodeJobs.push_back(m_threadPool->submit([this, job]() {
                job->decoded = decodeAndScale(job->compressed, job->customArtwork);
            }));
        }
    }

    if (pendingCount == 0) return;

    // 4. Finish decoding. wait() rather than get(): ThreadPool captures any
    //    exception into the future, and a job that failed simply leaves
    //    decoded.rgba null, which the upload loop already skips.
    const uint64_t tickDecodeStart = armGetSystemTick();
    if (m_threadPool) {
        for (auto& job : decodeJobs)
            job.wait();
    } else {
        for (size_t i = 0; i < pendingCount; ++i)
            pending[i].decoded = decodeAndScale(pending[i].compressed, pending[i].customArtwork);
    }

    // Compressed bytes are dead once decoded; release before the uploads.
    for (size_t i = 0; i < pendingCount; ++i)
        std::vector<uint8_t>().swap(pending[i].compressed);

    const uint64_t tickUploadStart = armGetSystemTick();

    // 5. Upload to GPU (must happen on the main/render thread) and
    //    wire the texture pointers on the corresponding GlossyIcons.

    // Pre-reserve pool capacity so emplace_back() never reallocates.
    // Reallocation would invalidate texture pointers already handed out
    // to GlossyIcon widgets earlier in this loop.
    {
        int newSlots = 0;
        int freeAvail = (int)m_freeSlots.size();
        for (size_t i = 0; i < pendingCount; ++i) {
            if (!pending[i].decoded.rgba) continue;
            if (freeAvail > 0) --freeAvail;
            else ++newSlots;
        }
        m_pool.reserve(m_pool.size() + newSlots);
    }

    for (size_t i = 0; i < pendingCount; ++i) {
        auto& d = pending[i].decoded;
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
            slot.appIndex = pending[i].appIndex;
            m_appToSlot[pending[i].appIndex] = poolIdx;
            if (pending[i].appIndex < (int)m_customArtwork.size())
                m_customArtwork[pending[i].appIndex] = pending[i].customArtwork;
            if (pending[i].appIndex < (int)allIcons.size())
                allIcons[pending[i].appIndex]->setTexture(&slot.texture);
            if (pending[i].appIndex < (int)allIcons.size())
                allIcons[pending[i].appIndex]->setCustomArtwork(pending[i].customArtwork);
        } else {
            // Silent until now, and the icon just stayed blank -- which is what
            // "some shortcuts were blank" on a 1TB card looks like from the
            // outside. The slot had already been taken off m_freeSlots and was
            // never recorded in m_appToSlot, so it leaked too: every failure
            // brought the next one closer. It goes back on the free list, and
            // says so, because a report of blank icons with nothing in the log
            // leaves nothing to work from.
            DebugLog::log("[streamer] UPLOAD FAILED app=%d %dx%d "
                          "(pool=%d free=%d) -- icon blank, slot returned",
                          pending[i].appIndex, d.w, d.h,
                          (int)m_pool.size(), (int)m_freeSlots.size());
            slot.appIndex = -1;
            if (pending[i].appIndex < (int)m_customArtwork.size())
                m_customArtwork[pending[i].appIndex] = false;
            m_freeSlots.push_back(poolIdx);
        }

        if (d.scaledWithMalloc) std::free(d.rgba);
        else stbi_image_free(d.rgba);
        d.rgba = nullptr;
    }

    // Timing breakdown so the split between SD reads, JPEG decoding and GPU
    // uploads is visible in menu.log. upload is the interesting one: every
    // texture currently costs a full queue waitIdle inside GpuDevice.
    const uint64_t tickEnd = armGetSystemTick();
    auto elapsedMs = [](uint64_t from, uint64_t to) -> unsigned {
        return static_cast<unsigned>(armTicksToNs(to - from) / 1000000ULL);
    };
    DebugLog::log("[streamer] page %d: %d icons [%d..%d) "
                  "fetch=%ums decode_wait=%ums upload=%ums total=%ums (%s)",
                  currentPage, (int)pendingCount, visibleStartApp, visibleEndApp,
                  elapsedMs(tickFetchStart, tickDecodeStart),
                  elapsedMs(tickDecodeStart, tickUploadStart),
                  elapsedMs(tickUploadStart, tickEnd),
                  elapsedMs(tickFetchStart, tickEnd),
                  m_threadPool ? "threaded" : "serial");
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
