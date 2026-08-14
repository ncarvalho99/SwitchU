#include "GameMetadataClient.hpp"

#include "themeshop/ThemeHttp.hpp"

#include <nlohmann/json.hpp>
#include <nxui/core/I18n.hpp>
#include <nxui/core/ThreadPool.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
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

bool isHttpsUrl(const std::string& value) {
    return value.rfind("https://", 0) == 0 && value.size() <= 2048;
}

std::vector<std::string> readStrings(const nlohmann::json& json, const char* key) {
    std::vector<std::string> result;
    const auto it = json.find(key);
    if (it == json.end() || !it->is_array()) return result;
    for (const auto& value : *it) {
        if (value.is_string()) result.push_back(value.get<std::string>());
    }
    return result;
}

float readNumber(const nlohmann::json& json, const char* key, float fallback = 0.f) {
    const auto it = json.find(key);
    if (it == json.end() || !it->is_number()) return fallback;
    const float value = it->get<float>();
    return std::isfinite(value) ? value : fallback;
}

} // namespace

void GameMetadataClient::load(nxui::ThreadPool& pool, std::string title) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const std::uint64_t revision = ++m_revision;
    m_snapshot = {};
    m_snapshot.phase = Phase::Loading;
    m_snapshot.revision = revision;
    m_future = pool.submit([this, title = std::move(title), revision]() {
        Snapshot next;
        try {
            next = fetch(title, revision);
        } catch (const std::exception& ex) {
            next.phase = Phase::Failed;
            next.error = ex.what();
            next.revision = revision;
        } catch (...) {
            next.phase = Phase::Failed;
            next.error = "Unknown metadata error";
            next.revision = revision;
        }
        std::lock_guard<std::mutex> resultLock(m_mutex);
        if (revision == m_revision)
            m_snapshot = std::move(next);
    });
}

GameMetadataClient::Snapshot GameMetadataClient::snapshot() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snapshot;
}

GameMetadataClient::Snapshot GameMetadataClient::fetch(std::string title, std::uint64_t revision) {
    const std::string query = "?title=" + encodeUrlComponent(title)
        + "&platform=nintendo-switch&language="
        + encodeUrlComponent(nxui::I18n::instance().activeLanguageTag());
    const nlohmann::json metadata = nlohmann::json::parse(
        themeshop::http::getText(std::string(kServiceUrl) + "/v1/metadata" + query));
    const nlohmann::json scores = nlohmann::json::parse(
        themeshop::http::getText(std::string(kServiceUrl) + "/v1/scores" + query));

    Snapshot result;
    result.phase = Phase::Ready;
    result.revision = revision;
    result.found = metadata.value("found", false);
    result.title = metadata.value("title", title);
    result.summary = metadata.value("summary", std::string());
    result.storyline = metadata.value("storyline", std::string());
    result.releaseDate = metadata.value("releaseDate", std::string());
    result.developers = readStrings(metadata, "developers");
    result.publishers = readStrings(metadata, "publishers");
    result.genres = readStrings(metadata, "genres");
    result.themes = readStrings(metadata, "themes");
    result.gameModes = readStrings(metadata, "gameModes");
    for (const auto& url : readStrings(metadata, "screenshots")) {
        if (isHttpsUrl(url)) result.screenshots.push_back(url);
    }

    const auto times = metadata.find("timeToBeat");
    if (times != metadata.end() && times->is_object()) {
        result.hoursHastily = readNumber(*times, "hastily");
        result.hoursMain = readNumber(*times, "main");
        result.hoursCompletionist = readNumber(*times, "completionist");
    }
    const auto metascore = scores.find("metascore");
    if (metascore != scores.end() && metascore->is_object()) {
        const float value = readNumber(*metascore, "value", -1.f);
        if (value >= 0.f && value <= 100.f) result.metascore = (int)std::lround(value);
    }
    const auto userscore = scores.find("userScore");
    if (userscore != scores.end() && userscore->is_object()) {
        const float value = readNumber(*userscore, "value", -1.f);
        if (value >= 0.f && value <= 10.f) result.userscore = value;
    }
    return result;
}
