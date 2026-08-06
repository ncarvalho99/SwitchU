#include "AppListLoader.hpp"
#include "core/DebugLog.hpp"
#include "smi_commands.hpp"
#include <switch.h>
#include <cstdio>
#include <vector>
#include <algorithm>
#ifdef SWITCHU_MENU
#include <switchu/control_cache.hpp>
#include <switchu/ns_ext.hpp>
#endif

namespace {

bool requiresInteractiveUserSelection(uint8_t account, uint8_t option) {
    return account == 1 && option == 0;
}

// Titles are drawn with TTF_RenderUTF8_Blended, which produces garbage glyphs
// for bytes that aren't valid UTF-8. One title rendering as noise while every
// other one is fine points at its NACP name rather than at the renderer, so
// dump the offending bytes instead of guessing at the cause.
bool isValidUtf8(const std::string& s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s.data());
    const size_t n = s.size();
    for (size_t i = 0; i < n;) {
        const unsigned char c = p[i];
        size_t extra;
        if (c < 0x80)                    extra = 0;
        else if ((c & 0xE0) == 0xC0)     extra = 1;
        else if ((c & 0xF0) == 0xE0)     extra = 2;
        else if ((c & 0xF8) == 0xF0)     extra = 3;
        else return false;
        if (i + extra >= n && extra > 0) return false;
        for (size_t k = 1; k <= extra; ++k)
            if ((p[i + k] & 0xC0) != 0x80) return false;
        i += extra + 1;
    }
    return true;
}

// Last line of defence. The daemon now refuses to cache an unrenderable NACP
// name, but a catalog written before that is still on disk, so replace anything
// that can't be drawn with the title ID rather than showing noise.
void sanitizeTitle(uint64_t titleId, std::string& name) {
    if (isValidUtf8(name))
        return;

    char hex[3 * 32 + 1] = {};
    const size_t shown = name.size() < 32 ? name.size() : 32;
    for (size_t i = 0; i < shown; ++i)
        std::snprintf(hex + i * 3, 4, "%02X ", (unsigned char)name[i]);
    DebugLog::log("[loader] title 0x%016lX has non-UTF8 name len=%zu, replacing. bytes: %s",
                  (unsigned long)titleId, name.size(), hex);

    char tidBuf[17];
    std::snprintf(tidBuf, sizeof(tidBuf), "%016lX", (unsigned long)titleId);
    name = tidBuf;
}

#ifdef SWITCHU_MENU
static constexpr s32 kMaxTrackedApplicationRecords = 1024;
static constexpr s32 kApplicationRecordChunkCount = 30;

bool listApplicationRecords(std::vector<switchu::ns::ExtApplicationRecord>& records) {
    records.clear();

    switchu::ns::ExtApplicationRecord chunk[kApplicationRecordChunkCount] = {};
    s32 offset = 0;
    while (offset < kMaxTrackedApplicationRecords) {
        s32 readCount = 0;
        Result rc = nsListApplicationRecord(
            reinterpret_cast<NsApplicationRecord*>(chunk),
            kApplicationRecordChunkCount,
            offset,
            &readCount);
        if (R_FAILED(rc)) {
            DebugLog::log("[loader] nsListApplicationRecord failed rc=0x%X offset=%d",
                          rc, offset);
            records.clear();
            return false;
        }
        if (readCount <= 0)
            break;

        const s32 remaining = kMaxTrackedApplicationRecords - offset;
        const s32 appendCount = readCount > remaining ? remaining : readCount;
        records.insert(records.end(), chunk, chunk + appendCount);
        offset += readCount;

        if (readCount < kApplicationRecordChunkCount)
            break;
    }

    std::sort(records.begin(), records.end(),
              [](const switchu::ns::ExtApplicationRecord& a,
                 const switchu::ns::ExtApplicationRecord& b) {
                  return a.id < b.id;
              });
    return true;
}

void queryApplicationViews(const std::vector<switchu::ns::ExtApplicationRecord>& records,
                           std::vector<switchu::ns::ExtApplicationView>& views) {
    views.clear();
    if (records.empty())
        return;

    std::vector<uint64_t> tids(records.size());
    for (size_t i = 0; i < records.size(); ++i)
        tids[i] = records[i].id;

    views.resize(records.size());
    Result rc = switchu::ns::queryApplicationViews(
        tids.data(),
        static_cast<int>(tids.size()),
        views.data());
    if (R_FAILED(rc)) {
        DebugLog::log("[loader] queryApplicationViews failed rc=0x%X", rc);
        std::fill(views.begin(), views.end(), switchu::ns::ExtApplicationView{});
    }
}
#endif

bool fetchDaemonCatalog(std::vector<PendingApp>& out) {
#ifdef SWITCHU_MENU
    std::vector<switchu::menu::smi_cmd::AppEntry> catalog;
    Result rc = switchu::menu::smi_cmd::getAppList(catalog, true);
    if (R_FAILED(rc) || catalog.empty()) {
        DebugLog::log("[loader] daemon catalog unavailable rc=0x%X count=%d",
                      rc, (int)catalog.size());
        return false;
    }

    char tidBuf[17];
    out.clear();
    out.reserve(catalog.size());
    for (auto& ent : catalog) {
        if (ent.titleId == 0 || ent.name.empty())
            continue;

        std::snprintf(tidBuf, sizeof(tidBuf), "%016lX", (unsigned long)ent.titleId);
        PendingApp a;
        a.id      = tidBuf;
        a.title   = std::move(ent.name);
        a.titleId = ent.titleId;
        a.viewFlags = ent.viewFlags;
        a.startupUserKnown = ent.startupUserKnown;
        a.startupUserAccount = ent.startupUserKnown ? ent.startupUserAccount : 1;
        a.startupUserAccountOption = ent.startupUserKnown ? ent.startupUserAccountOption : 0;
        a.userRequired = !ent.startupUserKnown ||
                         requiresInteractiveUserSelection(a.startupUserAccount,
                                                          a.startupUserAccountOption);
        // Icon bytes deliberately stay on disk here. IconStreamer pulls them
        // through AppListLoader::loadIconData for the visible page only —
        // reading every title's JPEG up front cost several MB of SD I/O on the
        // main thread before the first frame could be drawn.
        a.iconData = std::move(ent.icon);

        if (!a.startupUserKnown) {
            // The daemon resolves name and startup-user policy into the
            // catalog. Only titles it hasn't cached yet need a .meta read.
            switchu::control_cache::Meta meta{};
            if (switchu::control_cache::readMeta(ent.titleId, meta)) {
                if (meta.name[0] != '\0')
                    a.title = meta.name;
                a.startupUserKnown = true;
                a.startupUserAccount = meta.startup_user_account;
                a.startupUserAccountOption = meta.startup_user_account_option;
                a.userRequired = requiresInteractiveUserSelection(a.startupUserAccount,
                                                                  a.startupUserAccountOption);
            }
        }

        sanitizeTitle(a.titleId, a.title);
        out.push_back(std::move(a));
    }

    if (out.empty())
        return false;

    DebugLog::log("[loader] loaded %d apps from daemon catalog", (int)out.size());
    return true;
#else
    (void)out;
    return false;
#endif
}

void registerEntries(std::vector<PendingApp>& apps,
                     GridModel& model,
                     IconStreamer& streamer) {
    streamer.init((int)apps.size());
    streamer.setIconDataLoader(AppListLoader::loadIconData);
    for (int i = 0; i < (int)apps.size(); ++i) {
        auto& p = apps[i];
        streamer.setTitleId(i, p.titleId);
        if (!p.iconData.empty())
            streamer.setIconData(i, std::move(p.iconData));
        AppEntry entry;
        entry.id           = std::move(p.id);
        entry.title        = std::move(p.title);
        entry.titleId      = p.titleId;
        entry.iconTexIndex = -1;  // unused — IconStreamer handles textures
        entry.viewFlags    = p.viewFlags;
        entry.userRequired = p.userRequired;
        entry.startupUserKnown = p.startupUserKnown;
        entry.startupUserAccount = p.startupUserAccount;
        entry.startupUserAccountOption = p.startupUserAccountOption;
        entry.homebrewPath = std::move(p.homebrewPath);
        model.addEntry(std::move(entry));
    }
}

}

void AppListLoader::fetchApps() {
    char tidBuf[17];
    m_pending.clear();

#ifdef SWITCHU_HOMEBREW
    static const char* dummyNames[] = {
        "The Legend of Zelda: TotK",
        "Super Mario Odyssey",
        "Animal Crossing: NH",
        "Splatoon 3",
        "Mario Kart 8 Deluxe",
        "Super Smash Bros. Ultimate",
        "Pokemon Scarlet",
        "Fire Emblem Engage",
        "Xenoblade Chronicles 3",
        "Metroid Dread",
        "Kirby and the Forgotten Land",
        "Bayonetta 3",
        "Pikmin 4",
        "Luigi's Mansion 3",
        "Hollow Knight",
        "Celeste",
        "Stardew Valley",
        "Hades",
        "Undertale",
        "Minecraft",
    };
    constexpr int kDummyCount = sizeof(dummyNames) / sizeof(dummyNames[0]);
    for (int i = 0; i < kDummyCount; ++i) {
        uint64_t fakeTid = 0x0100000000010000ULL + (uint64_t)i;
        std::snprintf(tidBuf, sizeof(tidBuf), "%016lX", (unsigned long)fakeTid);
        PendingApp a;
        a.id      = tidBuf;
        a.title   = dummyNames[i];
        a.titleId = fakeTid;
        a.userRequired = true;
        a.startupUserAccount = 1;
        a.startupUserAccountOption = 0;
        uint32_t flags = (1u << 0) | (1u << 1) | (1u << 8);
        if (i == 5)  flags |= (1u << 6) | (1u << 7);
        if (i == 10) flags = (1u << 6);
        if (i == 15) flags = (1u << 13);
        a.viewFlags = flags;
        m_pending.push_back(std::move(a));
    }
    DebugLog::log("[loader] generated %d dummy apps", kDummyCount);

#else
    if (fetchDaemonCatalog(m_pending)) {
        DebugLog::log("[loader] fetched %d apps via daemon catalog",
                      (int)m_pending.size());
        return;
    }

#ifdef SWITCHU_MENU
    DebugLog::log("[loader] daemon catalog required; skipping menu-side app scan");
    return;
#endif

    std::vector<switchu::ns::ExtApplicationRecord> records;
    if (!listApplicationRecords(records))
        return;

    std::vector<switchu::ns::ExtApplicationView> views;
    queryApplicationViews(records, views);

    m_pending.reserve(records.size());

    for (size_t i = 0; i < records.size(); ++i) {
        uint64_t tid = records[i].id;
        std::snprintf(tidBuf, sizeof(tidBuf), "%016lX", (unsigned long)tid);

        uint32_t vf = views[i].flags;

        switchu::control_cache::Meta meta{};
        if (switchu::control_cache::readMeta(tid, meta)) {
            PendingApp a;
            a.id      = tidBuf;
            a.title   = meta.name;
            a.titleId = tid;
            a.viewFlags = vf;
            a.startupUserKnown = true;
            a.startupUserAccount = meta.startup_user_account;
            a.startupUserAccountOption = meta.startup_user_account_option;
            a.userRequired = requiresInteractiveUserSelection(a.startupUserAccount,
                                                              a.startupUserAccountOption);
            a.iconData = switchu::control_cache::readIcon(tid);
            m_pending.push_back(std::move(a));
            continue;
        }

        PendingApp a;
        a.id      = tidBuf;
        a.title   = tidBuf;
        a.titleId = tid;
        a.viewFlags = vf;
        a.startupUserKnown = false;
        a.startupUserAccount = 1;
        a.startupUserAccountOption = 0;
        a.userRequired = requiresInteractiveUserSelection(a.startupUserAccount, a.startupUserAccountOption);
        m_pending.push_back(std::move(a));
    }
#endif
    DebugLog::log("[loader] fetched %d apps", (int)m_pending.size());
}


std::vector<uint8_t> AppListLoader::loadIconData(uint64_t titleId) {
    std::vector<uint8_t> iconData;
    if (titleId == 0)
        return iconData;

#ifdef SWITCHU_MENU
    iconData = switchu::control_cache::readIcon(titleId);
#endif

    return iconData;
}


void AppListLoader::load(GridModel& model, IconStreamer& streamer) {
    fetchApps();
    if (m_pendingTransform)
        m_pendingTransform(m_pending);
    registerEntries(m_pending, model, streamer);
    m_pending.clear();
}


void AppListLoader::startAsync(nxui::ThreadPool& pool) {
    if (m_future.valid())
        m_future.get();

    m_future = pool.submit([this]() {
        fetchApps();
    });
}

bool AppListLoader::isReady() const {
    return m_future.valid() &&
           m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void AppListLoader::finalize(GridModel& model, IconStreamer& streamer) {
    if (m_future.valid())
        m_future.get();

    if (m_pendingTransform)
        m_pendingTransform(m_pending);
    registerEntries(m_pending, model, streamer);
    m_pending.clear();
}
