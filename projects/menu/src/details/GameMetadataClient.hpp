#pragma once

#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace nxui {
class ThreadPool;
}

// The console never talks to IGDB or Metacritic directly.  This small client
// consumes the normalised, cached SwitchU API contract so the credentials and
// source-specific parsers stay on the private service.
class GameMetadataClient {
public:
    enum class Phase { Idle, Loading, Ready, Failed };

    struct Snapshot {
        Phase phase = Phase::Idle;
        bool found = false;
        std::string title;
        std::string summary;
        std::string storyline;
        std::string releaseDate;
        std::vector<std::string> developers;
        std::vector<std::string> publishers;
        std::vector<std::string> genres;
        std::vector<std::string> themes;
        std::vector<std::string> gameModes;
        std::vector<std::string> screenshots;
        float hoursHastily = 0.f;
        float hoursMain = 0.f;
        float hoursCompletionist = 0.f;
        int metascore = -1;
        float userscore = -1.f;
        std::string error;
        std::uint64_t revision = 0;
    };

    static constexpr const char* kServiceUrl = "https://switchu-api.nclabs.dev";

    void load(nxui::ThreadPool& pool, std::string title);
    Snapshot snapshot() const;

private:
    static Snapshot fetch(std::string title, std::uint64_t revision);

    mutable std::mutex m_mutex;
    Snapshot m_snapshot;
    std::future<void> m_future;
    std::uint64_t m_revision = 0;
};
