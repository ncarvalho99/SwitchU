#include "YouTubeClient.hpp"

#include "ThemeHttp.hpp"
#include "core/DebugLog.hpp"

#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/ThreadPool.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace {

std::string percentEncode(const std::string& value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 2);
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 15]);
        }
    }
    return out;
}

std::string normalizedComparison(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c))
            out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

} // namespace

YouTubeClient::YouTubeClient() = default;

YouTubeClient::~YouTubeClient() {
    cancelSearch();
}

std::string YouTubeClient::musicDirectory() {
    return "sdmc:/config/SwitchU/music";
}

std::string YouTubeClient::sanitizeFilename(const std::string& name) {
    std::string clean;
    clean.reserve(name.size());
    for (char c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == '\n' || c == '\r' || c == '\t') {
            clean.push_back('_');
        } else if (static_cast<unsigned char>(c) >= 32) {
            clean.push_back(c);
        }
    }
    while (!clean.empty() && (clean.back() == ' ' || clean.back() == '.' || clean.back() == '_'))
        clean.pop_back();
    if (clean.empty())
        clean = "track";
    if (clean.size() > 80)
        clean.resize(80);
    return clean;
}

std::vector<YouTubeClient::TrackItem> YouTubeClient::tracksSnapshot() const {
    std::lock_guard<std::mutex> lk(m_tracksMutex);
    return m_tracks;
}

size_t YouTubeClient::trackCount() const {
    std::lock_guard<std::mutex> lk(m_tracksMutex);
    return m_tracks.size();
}

bool YouTubeClient::getTrack(size_t index, TrackItem& outTrack) const {
    std::lock_guard<std::mutex> lk(m_tracksMutex);
    if (index < m_tracks.size()) {
        outTrack = m_tracks[index];
        return true;
    }
    return false;
}

const YouTubeClient::TrackItem* YouTubeClient::trackAt(size_t index) const {
    std::lock_guard<std::mutex> lk(m_tracksMutex);
    if (index < m_tracks.size()) {
        return &m_tracks[index];
    }
    return nullptr;
}

void YouTubeClient::cancelSearch() {
    m_cancelRequested.store(true, std::memory_order_release);
}

void YouTubeClient::search(const std::string& query, SearchCallback cb) {
    if (m_isSearching.load(std::memory_order_acquire)) {
        if (cb) cb(false, "Already searching");
        return;
    }

    m_lastQuery = query;
    m_isSearching.store(true, std::memory_order_release);
    m_cancelRequested.store(false, std::memory_order_release);

    auto task = [this, query, cb]() {
        DebugLog::log("[youtube] Starting search for: '%s'", query.c_str());
        std::vector<TrackItem> items;
        std::string err;

        // 1. Try backend service first
        try {
            items = searchBackend(query);
            DebugLog::log("[youtube] Backend returned %zu results", items.size());
        } catch (const std::exception& ex) {
            DebugLog::log("[youtube] Backend search failed: %s, falling back to InnerTube", ex.what());
        } catch (...) {
            DebugLog::log("[youtube] Backend search failed with unknown exception");
        }

        // 2. Fall back to direct YouTube InnerTube search if backend returned nothing
        if (items.empty() && !m_cancelRequested.load(std::memory_order_acquire)) {
            try {
                items = searchInnerTube(query);
                DebugLog::log("[youtube] InnerTube returned %zu results", items.size());
            } catch (const std::exception& ex) {
                err = ex.what();
                DebugLog::log("[youtube] InnerTube search failed: %s", ex.what());
            } catch (...) {
                err = "Unknown search error";
            }
        }

        if (m_cancelRequested.load(std::memory_order_acquire)) {
            m_isSearching.store(false, std::memory_order_release);
            if (cb) cb(false, "Search cancelled");
            return;
        }

        {
            std::lock_guard<std::mutex> lk(m_tracksMutex);
            m_tracks = std::move(items);
        }

        refreshDownloadedStatus();

        m_isSearching.store(false, std::memory_order_release);
        if (cb) {
            cb(err.empty(), err);
        }
    };

    if (m_pool) {
        m_pool->submit(std::move(task));
    } else {
        std::thread(std::move(task)).detach();
    }
}

std::vector<YouTubeClient::TrackItem> YouTubeClient::searchBackend(const std::string& query) {
    std::string url = m_backendUrl + "/api/search?q=" + percentEncode(query) + "&limit=24";
    std::string resp = themeshop::http::getText(url);
    if (resp.empty())
        return {};

    auto root = nlohmann::json::parse(resp);
    if (!root.contains("items") || !root["items"].is_array())
        return {};

    std::vector<TrackItem> items;
    for (const auto& it : root["items"]) {
        TrackItem track;
        track.id = it.value("id", "");
        track.title = it.value("title", "");
        track.author = it.value("author", "");
        track.duration = it.value("duration", "0:00");
        track.thumbnailUrl = it.value("thumbnail", "");
        if (!track.id.empty() && !track.title.empty()) {
            items.push_back(std::move(track));
        }
    }
    return items;
}

std::vector<YouTubeClient::TrackItem> YouTubeClient::searchInnerTube(const std::string& query) {
    std::string url = "https://www.youtube.com/youtubei/v1/search";
    nlohmann::json payload;
    payload["context"]["client"]["clientName"] = "WEB";
    payload["context"]["client"]["clientVersion"] = "2.20240101.00.00";
    payload["context"]["client"]["hl"] = "en";
    payload["context"]["client"]["gl"] = "US";
    payload["query"] = query;

    std::string resp = themeshop::http::postJson(url, payload.dump());
    if (resp.empty())
        return {};

    auto root = nlohmann::json::parse(resp);
    std::vector<TrackItem> items;

    try {
        const auto& contents = root["contents"]["twoColumnSearchResultsRenderer"]["primaryContents"]["sectionListRenderer"]["contents"];
        for (const auto& section : contents) {
            if (!section.contains("itemSectionRenderer") || !section["itemSectionRenderer"].contains("contents"))
                continue;
            const auto& itemSection = section["itemSectionRenderer"]["contents"];
            for (const auto& item : itemSection) {
                if (!item.contains("videoRenderer"))
                    continue;
                const auto& vr = item["videoRenderer"];
                TrackItem track;
                track.id = vr.value("videoId", "");
                if (track.id.empty())
                    continue;

                if (vr.contains("title")) {
                    if (vr["title"].contains("runs") && vr["title"]["runs"].is_array() && !vr["title"]["runs"].empty()) {
                        track.title = vr["title"]["runs"][0].value("text", "");
                    } else {
                        track.title = vr["title"].value("simpleText", "");
                    }
                }

                if (vr.contains("ownerText") && vr["ownerText"].contains("runs") && !vr["ownerText"]["runs"].empty()) {
                    track.author = vr["ownerText"]["runs"][0].value("text", "");
                } else if (vr.contains("shortBylineText") && vr["shortBylineText"].contains("runs") && !vr["shortBylineText"]["runs"].empty()) {
                    track.author = vr["shortBylineText"]["runs"][0].value("text", "");
                }

                if (vr.contains("lengthText")) {
                    track.duration = vr["lengthText"].value("simpleText", "0:00");
                } else {
                    track.duration = "0:00";
                }

                if (vr.contains("thumbnail") && vr["thumbnail"].contains("thumbnails") && vr["thumbnail"]["thumbnails"].is_array() && !vr["thumbnail"]["thumbnails"].empty()) {
                    track.thumbnailUrl = vr["thumbnail"]["thumbnails"].back().value("url", "");
                }
                if (track.thumbnailUrl.empty()) {
                    track.thumbnailUrl = "https://i.ytimg.com/vi/" + track.id + "/hqdefault.jpg";
                }

                if (!track.id.empty() && !track.title.empty()) {
                    items.push_back(std::move(track));
                    if (items.size() >= 24)
                        break;
                }
            }
            if (items.size() >= 24)
                break;
        }
    } catch (const std::exception& ex) {
        DebugLog::log("[youtube] InnerTube parse warning: %s", ex.what());
    }

    return items;
}

std::string YouTubeClient::resolveStreamUrl(const std::string& videoId, const std::string& /*title*/) {
    // 1. Ask backend /api/stream?id=...
    try {
        std::string url = m_backendUrl + "/api/stream?id=" + videoId;
        std::string resp = themeshop::http::getText(url);
        if (!resp.empty()) {
            auto root = nlohmann::json::parse(resp);
            if (root.value("status", "") == "ok" && root.contains("stream_url")) {
                std::string streamUrl = root.value("stream_url", "");
                if (!streamUrl.empty())
                    return streamUrl;
            }
        }
    } catch (const std::exception& ex) {
        DebugLog::log("[youtube] resolveStreamUrl backend query failed: %s", ex.what());
    }

    // 2. Direct download endpoint on backend which redirects
    return m_backendUrl + "/api/download?id=" + videoId;
}

void YouTubeClient::refreshDownloadedStatus() {
    std::vector<std::string> existingBasenames;
    std::error_code ec;
    std::string musicDir = musicDirectory();
    if (std::filesystem::is_directory(musicDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(musicDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ext == ".mp3" || ext == ".ogg" || ext == ".wav") {
                existingBasenames.push_back(normalizedComparison(entry.path().stem().string()));
            }
        }
    }

    std::lock_guard<std::mutex> lk(m_tracksMutex);
    for (auto& track : m_tracks) {
        std::string cleanNorm = normalizedComparison(sanitizeFilename(track.title));
        bool found = false;
        for (const auto& existing : existingBasenames) {
            if (existing == cleanNorm || existing.find(cleanNorm) != std::string::npos || cleanNorm.find(existing) != std::string::npos) {
                found = true;
                break;
            }
        }
        track.isDownloaded = found;
        if (found) {
            track.statusText = "Downloaded";
        }
    }
}

bool YouTubeClient::downloadTrack(size_t trackIndex, ProgressCallback onProgress, CompleteCallback onComplete) {
    if (m_isDownloading.exchange(true)) {
        if (onComplete) onComplete(false, "", "Another download is already in progress");
        return false;
    }

    TrackItem track;
    {
        std::lock_guard<std::mutex> lk(m_tracksMutex);
        if (trackIndex >= m_tracks.size()) {
            m_isDownloading.store(false);
            if (onComplete) onComplete(false, "", "Invalid track index");
            return false;
        }
        m_tracks[trackIndex].isDownloading = true;
        m_tracks[trackIndex].downloadProgress = 0.01f;
        track = m_tracks[trackIndex];
    }

    auto task = [this, trackIndex, track, onProgress, onComplete]() {
        std::string cleanName = sanitizeFilename(track.title);
        std::string musicDir = musicDirectory();
        std::error_code ec;
        std::filesystem::create_directories(musicDir, ec);

        std::string destPath = musicDir + "/" + cleanName + ".mp3";
        DebugLog::log("[youtube] Starting download for '%s' to '%s'", track.title.c_str(), destPath.c_str());

        bool success = false;
        std::string errorMsg;

        try {
            std::string streamUrl = resolveStreamUrl(track.id, track.title);
            DebugLog::log("[youtube] Resolved stream URL: %s", streamUrl.c_str());

            auto progressCb = [this, trackIndex, onProgress](std::uint64_t dl, std::uint64_t total) {
                float p = (total > 0) ? static_cast<float>(dl) / static_cast<float>(total) : 0.0f;
                {
                    std::lock_guard<std::mutex> lk(m_tracksMutex);
                    if (trackIndex < m_tracks.size()) {
                        m_tracks[trackIndex].downloadProgress = p;
                    }
                }
                if (onProgress) {
                    onProgress(p, dl, total);
                }
            };

            std::uint64_t written = themeshop::http::getToFile(streamUrl, destPath, progressCb);
            DebugLog::log("[youtube] Download finished: %llu bytes written to %s",
                          static_cast<unsigned long long>(written), destPath.c_str());
            success = (written > 0);
        } catch (const std::exception& ex) {
            errorMsg = ex.what();
            DebugLog::log("[youtube] Download failed: %s", ex.what());
            std::remove(destPath.c_str());
        } catch (...) {
            errorMsg = "Unknown download failure";
            std::remove(destPath.c_str());
        }

        {
            std::lock_guard<std::mutex> lk(m_tracksMutex);
            if (trackIndex < m_tracks.size()) {
                m_tracks[trackIndex].isDownloading = false;
                if (success) {
                    m_tracks[trackIndex].isDownloaded = true;
                    m_tracks[trackIndex].downloadProgress = 1.0f;
                    m_tracks[trackIndex].localFilePath = destPath;
                } else {
                    m_tracks[trackIndex].downloadProgress = 0.0f;
                }
            }
        }

        m_isDownloading.store(false);
        if (onComplete) {
            onComplete(success, success ? destPath : "", errorMsg);
        }
    };

    if (m_pool) {
        m_pool->submit(std::move(task));
    } else {
        std::thread(std::move(task)).detach();
    }

    return true;
}

YouTubeClient::PreviewPhase YouTubeClient::thumbnailPhase(const std::string& videoId) const {
    if (videoId.empty())
        return PreviewPhase::Failed;
    std::lock_guard<std::mutex> lk(m_thumbMutex);
    auto it = m_thumbnailCache.find(videoId);
    if (it == m_thumbnailCache.end() || !it->second)
        return PreviewPhase::Idle;
    std::lock_guard<std::mutex> innerLk(it->second->mutex);
    return it->second->phase;
}

const nxui::Texture* YouTubeClient::thumbnailTexture(const std::string& videoId) const {
    if (videoId.empty())
        return nullptr;
    std::lock_guard<std::mutex> lk(m_thumbMutex);
    auto it = m_thumbnailCache.find(videoId);
    if (it == m_thumbnailCache.end() || !it->second)
        return nullptr;
    std::lock_guard<std::mutex> innerLk(it->second->mutex);
    if (it->second->phase != PreviewPhase::Ready || !it->second->texture.valid())
        return nullptr;
    return &it->second->texture;
}

void YouTubeClient::primeThumbnail(const std::string& videoId, const std::string& thumbnailUrl) {
    if (videoId.empty() || thumbnailUrl.empty())
        return;

    std::shared_ptr<PreviewState> state;
    {
        std::lock_guard<std::mutex> lk(m_thumbMutex);
        auto& entry = m_thumbnailCache[videoId];
        if (!entry) {
            entry = std::make_shared<PreviewState>();
        }
        state = entry;
        state->lastAccessed = ++m_accessCounter;
    }

    {
        std::lock_guard<std::mutex> innerLk(state->mutex);
        if (state->phase != PreviewPhase::Idle)
            return;
        state->phase = PreviewPhase::Loading;
    }

    auto fetchTask = [state, videoId, thumbnailUrl]() {
        try {
            auto bytes = themeshop::http::getBytes(thumbnailUrl);
            std::lock_guard<std::mutex> innerLk(state->mutex);
            if (!bytes.empty()) {
                state->bytes = std::move(bytes);
                state->phase = PreviewPhase::Downloaded;
            } else {
                state->phase = PreviewPhase::Failed;
            }
        } catch (...) {
            std::lock_guard<std::mutex> innerLk(state->mutex);
            state->phase = PreviewPhase::Failed;
        }
    };

    if (m_pool) {
        m_pool->submit(std::move(fetchTask));
    } else {
        std::thread(std::move(fetchTask)).detach();
    }
}

void YouTubeClient::updateThumbnailTextures() {
    if (!m_gpu || !m_renderer)
        return;

    std::lock_guard<std::mutex> lk(m_thumbMutex);
    for (auto& pair : m_thumbnailCache) {
        auto& state = pair.second;
        if (!state) continue;

        std::lock_guard<std::mutex> innerLk(state->mutex);
        if (state->phase == PreviewPhase::Downloaded && !state->bytes.empty()) {
            nxui::Texture tex;
            bool ok = tex.loadFromMemory(*m_gpu, *m_renderer, state->bytes.data(), state->bytes.size(), 320);
            state->bytes.clear();
            state->bytes.shrink_to_fit();
            if (ok) {
                state->texture = std::move(tex);
                state->phase = PreviewPhase::Ready;
            } else {
                state->phase = PreviewPhase::Failed;
            }
        }
    }
}

void YouTubeClient::trimThumbnailCache(size_t maxItems) {
    std::lock_guard<std::mutex> lk(m_thumbMutex);
    if (m_thumbnailCache.size() <= maxItems)
        return;

    std::vector<std::pair<std::string, std::uint64_t>> entries;
    entries.reserve(m_thumbnailCache.size());
    for (const auto& pair : m_thumbnailCache) {
        if (pair.second) {
            entries.emplace_back(pair.first, pair.second->lastAccessed);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
        return a.second < b.second; // oldest first
    });

    size_t toRemove = m_thumbnailCache.size() - maxItems;
    for (size_t i = 0; i < toRemove && i < entries.size(); ++i) {
        m_thumbnailCache.erase(entries[i].first);
    }
}
