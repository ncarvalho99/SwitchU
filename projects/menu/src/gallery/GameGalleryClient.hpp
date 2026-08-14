#pragma once

#include <cstdint>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace nxui {
class ThreadPool;
}

class GameGalleryClient {
public:
    enum class Category { Grids, Heroes };
    enum class Phase { Idle, Loading, Ready, Failed };

    struct Asset {
        std::uint64_t id = 0;
        std::string url;
        std::string thumb;
        std::string author;
        std::string style;
        int width = 0;
        int height = 0;
    };

    struct Snapshot {
        Phase phase = Phase::Idle;
        Category category = Category::Grids;
        std::uint64_t gameId = 0;
        std::string matchedName;
        std::string error;
        std::vector<Asset> assets;
        std::uint64_t revision = 0;
    };

    static constexpr const char* kServiceUrl = "https://gallery.nclabs.dev";

    void load(nxui::ThreadPool& pool, std::string title, Category category,
              std::string dimensions);
    Snapshot snapshot() const;

private:
    static Snapshot fetch(std::string title, Category category, std::string dimensions,
                          std::uint64_t revision);

    mutable std::mutex m_mutex;
    Snapshot m_snapshot;
    std::future<void> m_future;
    std::uint64_t m_revision = 0;
};
