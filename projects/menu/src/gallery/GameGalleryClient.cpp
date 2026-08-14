#include "GameGalleryClient.hpp"

#include "themeshop/ThemeHttp.hpp"

#include <nlohmann/json.hpp>
#include <nxui/core/ThreadPool.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace {

std::string encodeUrlComponent(const std::string& text) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(text.size() * 3);
    for (unsigned char ch : text) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
            out.push_back((char)ch);
        } else {
            out += '%';
            out += hex[ch >> 4];
            out += hex[ch & 0x0F];
        }
    }
    return out;
}

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return (char)std::tolower(ch);
    });
    return value;
}

bool isHttpsUrl(const std::string& value) {
    return value.rfind("https://", 0) == 0 && value.size() <= 2048;
}

std::uint64_t readId(const nlohmann::json& value) {
    if (value.is_number_unsigned()) return value.get<std::uint64_t>();
    if (value.is_number_integer()) return (std::uint64_t)std::max<std::int64_t>(0, value.get<std::int64_t>());
    return 0;
}

} // namespace

void GameGalleryClient::load(nxui::ThreadPool& pool, std::string title, Category category,
                             std::string dimensions) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t revision = ++m_revision;
    m_snapshot = {};
    m_snapshot.phase = Phase::Loading;
    m_snapshot.category = category;
    m_snapshot.revision = revision;

    m_future = pool.submit([this, title = std::move(title), category,
                            dimensions = std::move(dimensions), revision]() {
        Snapshot next;
        try {
            next = fetch(title, category, dimensions, revision);
        } catch (const std::exception& ex) {
            next.phase = Phase::Failed;
            next.category = category;
            next.revision = revision;
            next.error = ex.what();
        } catch (...) {
            next.phase = Phase::Failed;
            next.category = category;
            next.revision = revision;
            next.error = "Unknown gallery error";
        }

        std::lock_guard<std::mutex> resultLock(m_mutex);
        if (revision == m_revision)
            m_snapshot = std::move(next);
    });
}

GameGalleryClient::Snapshot GameGalleryClient::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

GameGalleryClient::Snapshot GameGalleryClient::fetch(std::string title,
                                                      Category category,
                                                      std::string dimensions,
                                                      std::uint64_t revision) {
    const std::string searchBody = themeshop::http::getText(
        std::string(kServiceUrl) + "/v1/search?query=" + encodeUrlComponent(title));
    const nlohmann::json search = nlohmann::json::parse(searchBody);
    const auto gamesIt = search.find("games");
    if (gamesIt == search.end() || !gamesIt->is_array() || gamesIt->empty())
        throw std::runtime_error("No SteamGridDB match found");

    const std::string wanted = lowerAscii(title);
    const nlohmann::json* game = &gamesIt->front();
    for (const auto& candidate : *gamesIt) {
        if (candidate.value("name", std::string()) == title) {
            game = &candidate;
            break;
        }
        if (lowerAscii(candidate.value("name", std::string())) == wanted)
            game = &candidate;
    }

    const std::uint64_t gameId = readId(game->value("id", nlohmann::json()));
    if (gameId == 0)
        throw std::runtime_error("SteamGridDB match has no valid id");

    const char* kind = category == Category::Grids ? "grids" : "heroes";
    const std::string assetBody = themeshop::http::getText(
        std::string(kServiceUrl) + "/v1/games/" + std::to_string(gameId) + "/" + kind
        + "?dimensions=" + encodeUrlComponent(dimensions));
    const nlohmann::json assetJson = nlohmann::json::parse(assetBody);
    const auto assetsIt = assetJson.find(kind);
    if (assetsIt == assetJson.end() || !assetsIt->is_array())
        throw std::runtime_error("Gallery response is invalid");

    Snapshot result;
    result.phase = Phase::Ready;
    result.category = category;
    result.revision = revision;
    result.gameId = gameId;
    result.matchedName = game->value("name", title);
    for (const auto& item : *assetsIt) {
        const std::string url = item.value("url", std::string());
        const std::string thumb = item.value("thumb", url);
        if (!isHttpsUrl(url) || !isHttpsUrl(thumb))
            continue;

        Asset asset;
        asset.id = readId(item.value("id", nlohmann::json()));
        asset.url = url;
        asset.thumb = thumb;
        asset.author = item.value("author", std::string());
        asset.style = item.value("style", std::string());
        asset.width = item.value("width", 0);
        asset.height = item.value("height", 0);
        result.assets.push_back(std::move(asset));
    }
    return result;
}
