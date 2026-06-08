#include "AppListLoader.hpp"
#include "core/DebugLog.hpp"
#include "smi_commands.hpp"
#include <switch.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <algorithm>
#ifdef SWITCHU_MENU
#include <nxtc.h>
#include <switchu/ns_ext.hpp>
#endif

namespace {

bool requiresInteractiveUserSelection(uint8_t account, uint8_t option) {
    return account == 1 && option == 0;
}

#ifdef SWITCHU_MENU
static NsApplicationControlData g_controlData;

struct StartupUserInfo {
    uint8_t account = 1;
    uint8_t option = 0;
};

StartupUserInfo queryStartupUserInfoImpl(uint64_t tid, StartupUserInfo fallback = {}) {
    size_t controlSize = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid,
                                            &g_controlData, sizeof(g_controlData), &controlSize);
    if (R_FAILED(rc)) {
        DebugLog::log("[loader] startup_user query failed tid=%016lX rc=0x%X",
                      (unsigned long)tid, rc);
        return fallback;
    }
    StartupUserInfo info{};
    info.account = g_controlData.nacp.startup_user_account;
    info.option = g_controlData.nacp.startup_user_account_option;
    return info;
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
        a.iconData = std::move(ent.icon);
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
        model.addEntry(std::move(entry));
    }
}

}

bool AppListLoader::queryStartupUserInfo(uint64_t titleId, uint8_t& account, uint8_t& option) {
#ifdef SWITCHU_MENU
    StartupUserInfo info = queryStartupUserInfoImpl(titleId);
    account = info.account;
    option = info.option;
    return true;
#else
    (void)titleId;
    account = 1;
    option = 0;
    return true;
#endif
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

    NsApplicationRecord records[1024] = {};
    s32 recordCount = 0;
    nsListApplicationRecord(records, 1024, 0, &recordCount);

    static switchu::ns::ExtApplicationView views[1024] = {};
    {
        uint64_t tids[1024];
        for (int i = 0; i < recordCount && i < 1024; ++i)
            tids[i] = records[i].application_id;
        if (recordCount > 0)
            switchu::ns::queryApplicationViews(tids, recordCount, views);
    }

    m_pending.reserve(recordCount);

    for (int i = 0; i < recordCount; ++i) {
        uint64_t tid = records[i].application_id;
        std::snprintf(tidBuf, sizeof(tidBuf), "%016lX", (unsigned long)tid);

        uint32_t vf = views[i].flags;

        NxTitleCacheApplicationMetadata* meta = nxtcGetApplicationMetadataEntryById(tid);
        if (meta) {
            StartupUserInfo startup{};
            PendingApp a;
            a.id      = tidBuf;
            a.title   = meta->name ? meta->name : "";
            a.titleId = tid;
            a.viewFlags = vf;
            a.startupUserKnown = !m_fastStartupUserInfo;
            if (!m_fastStartupUserInfo)
                startup = queryStartupUserInfoImpl(tid);
            a.startupUserAccount = startup.account;
            a.startupUserAccountOption = startup.option;
            a.userRequired = requiresInteractiveUserSelection(startup.account, startup.option);
            m_pending.push_back(std::move(a));
            nxtcFreeApplicationMetadata(&meta);
            continue;
        }

        size_t controlSize = 0;
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, tid,
                                                &g_controlData, sizeof(g_controlData), &controlSize);
        if (R_FAILED(rc)) continue;

        NacpLanguageEntry* langEntry = nullptr;
        rc = nacpGetLanguageEntry(&g_controlData.nacp, &langEntry);
        if (R_FAILED(rc) || !langEntry || langEntry->name[0] == '\0') {
            langEntry = nullptr;
            for (int l = 0; l < 16; ++l) {
                NacpLanguageEntry* e = &g_controlData.nacp.lang[l];
                if (e->name[0] != '\0') { langEntry = e; break; }
            }
        }
        if (!langEntry || langEntry->name[0] == '\0') continue;

        size_t iconSize = controlSize - sizeof(NacpStruct);
        nxtcAddEntry(tid, &g_controlData.nacp, iconSize,
                     iconSize > 0 ? g_controlData.icon : nullptr, false);

        PendingApp a;
        a.id      = tidBuf;
        a.title   = langEntry->name;
        a.titleId = tid;
        a.viewFlags = vf;
        a.startupUserKnown = true;
        a.startupUserAccount = g_controlData.nacp.startup_user_account;
        a.startupUserAccountOption = g_controlData.nacp.startup_user_account_option;
        a.userRequired = requiresInteractiveUserSelection(a.startupUserAccount, a.startupUserAccountOption);
        m_pending.push_back(std::move(a));
    }
    nxtcFlushCacheFile();
#endif
    DebugLog::log("[loader] fetched %d apps (fast_startup_user=%d)",
                  (int)m_pending.size(), m_fastStartupUserInfo ? 1 : 0);
}


std::vector<uint8_t> AppListLoader::loadIconData(uint64_t titleId) {
    std::vector<uint8_t> iconData;
    if (titleId == 0)
        return iconData;

#ifdef SWITCHU_MENU
    NxTitleCacheApplicationMetadata* meta = nxtcGetApplicationMetadataEntryById(titleId);
    if (meta) {
        if (meta->icon_data && meta->icon_size > 0) {
            auto* ptr = static_cast<const uint8_t*>(meta->icon_data);
            iconData.assign(ptr, ptr + meta->icon_size);
        }
        nxtcFreeApplicationMetadata(&meta);
        if (!iconData.empty())
            return iconData;
    }

    size_t controlSize = 0;
    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId,
                                            &g_controlData, sizeof(g_controlData), &controlSize);
    if (R_SUCCEEDED(rc) && controlSize > sizeof(NacpStruct)) {
        const size_t iconSize = controlSize - sizeof(NacpStruct);
        iconData.assign(g_controlData.icon, g_controlData.icon + iconSize);
    }
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
