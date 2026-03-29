#pragma once
#include "core/GridModel.hpp"
#include "launcher/IconStreamer.hpp"
#include <nxui/core/Texture.hpp>
#include <nxui/core/GpuDevice.hpp>
#include <nxui/core/Renderer.hpp>
#include <nxui/core/ThreadPool.hpp>
#include <vector>
#include <string>
#include <future>
#include <cstdint>

struct PendingApp {
    std::string         id;
    std::string         title;
    uint64_t            titleId = 0;
    uint32_t            viewFlags = 0;
    std::vector<uint8_t> iconData;
};

class AppListLoader {
public:
    // Streaming path: fetch apps and hand compressed icon data to the streamer.
    void load(GridModel& model, IconStreamer& streamer);

    void startAsync(nxui::ThreadPool& pool);

    bool isReady() const;

    void finalize(GridModel& model, IconStreamer& streamer);

private:
    void fetchApps();

    std::future<void>       m_future;
    std::vector<PendingApp> m_pending;
};
