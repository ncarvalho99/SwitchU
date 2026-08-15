// In-launcher updates: ask GitHub what the newest release is, show what it
// changed, and install it when the player accepts.
//
// The payload is downloaded and verified in full before a single file is
// replaced, because the archive being unpacked contains the menu that is doing
// the unpacking. A half-written download must never reach the card.

#include "WiiUMenuApp.hpp"

#include "themeshop/ThemeHttp.hpp"
#include "themeshop/ZipReader.hpp"
#include "core/DebugLog.hpp"

#include <nxui/core/I18n.hpp>

#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>
#include <mutex>
#include <string>

#ifndef SWITCHU_VERSION
#define SWITCHU_VERSION "unknown"
#endif

namespace {

constexpr const char* kUpdateDir = "sdmc:/config/SwitchU/update";
constexpr const char* kUpdateArchive = "sdmc:/config/SwitchU/update/update.zip";
// The archive lays out atmosphere/ and switch/ exactly as they sit on the card,
// so the card root is the extraction target.
constexpr const char* kCardRoot = "sdmc:/";

// The staging directory does not exist on a fresh card, and curl will not
// create it. Only this one level is needed: its parent is the config directory
// the menu already owns.
bool ensureUpdateDirectory() {
    struct stat info {};
    if (stat(kUpdateDir, &info) == 0)
        return true;
    return mkdir(kUpdateDir, 0777) == 0;
}

int todayNumber() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return (int)std::chrono::duration_cast<std::chrono::hours>(now).count() / 24;
}

// Notes for the version that is installed, shipped with the build. The tab can
// answer "what changed" before any network call, and a console that never
// reaches GitHub still gets an answer. tools/export_release_notes.py derives
// this from CHANGELOG.md during release preparation.
std::string loadBundledReleaseNotes(const std::string& languageTag) {
    std::ifstream file(std::string(SD_ASSETS) + "/notes/release-notes.txt");
    if (!file)
        return {};
    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    constexpr const char* kMarker = "--pt-BR--";
    const std::size_t split = text.find(kMarker);
    if (split == std::string::npos)
        return text;
    if (languageTag.rfind("pt", 0) == 0)
        return text.substr(split + std::char_traits<char>::length(kMarker));
    return text.substr(0, split);
}

} // namespace

struct WiiUMenuApp::UpdateDownload {
    std::mutex mutex;
    update::UpdateClient::Release release;
    std::uint64_t received = 0;
    std::uint64_t total = 0;
    bool extracting = false;
    bool done = false;
    bool ok = false;
    std::string error;
};

void WiiUMenuApp::startUpdateCheck(bool forced) {
#ifdef SWITCHU_MENU
    if (!forced) {
        const int today = todayNumber();
        // Once a day. The menu restarts every time a game is closed, and asking
        // GitHub on each of those would burn the unauthenticated rate limit for
        // no new information.
        if (m_config.lastUpdateCheckDay == today)
            return;
        m_config.lastUpdateCheckDay = today;
        m_config.save();
    }
    m_updateOffered = false;
    m_updateCheckForced = forced;
    m_updateStatus = nxui::I18n::instance().tr("settings.about.check_updates_running",
                                               "Checking for updates...");
    publishUpdateState();
    m_updateClient.check(m_threadPool, SWITCHU_VERSION);
    DebugLog::log("[update] check started (current %s)", SWITCHU_VERSION);
#else
    (void)forced;
#endif
}

void WiiUMenuApp::publishUpdateState() {
#ifdef SWITCHU_MENU
    if (!m_themeShop)
        return;
    const std::string language = nxui::I18n::instance().activeLanguageTag();
    const bool havePublished = !m_latestRelease.notes.empty();
    m_themeShop->setUpdateState(
        std::string("SwitchU ") + SWITCHU_VERSION,
        m_pendingUpdate.version,
        m_updateStatus,
        m_updateDownload != nullptr,
        havePublished ? m_latestRelease.version : std::string(SWITCHU_VERSION),
        havePublished ? update::UpdateClient::condenseNotes(m_latestRelease.notes, language)
                      : loadBundledReleaseNotes(language));
#endif
}

void WiiUMenuApp::syncUpdateCheck() {
#ifdef SWITCHU_MENU
    const auto snapshot = m_updateClient.snapshot();
    if (snapshot.revision == m_updateSeenRevision)
        return;
    if (snapshot.phase == update::UpdateClient::Phase::Checking)
        return;
    m_updateSeenRevision = snapshot.revision;

    auto& i18n = nxui::I18n::instance();
    // A check the player asked for has to answer, even when the answer is that
    // nothing changed. The daily one stays silent unless there is news.
    if (snapshot.phase == update::UpdateClient::Phase::Failed) {
        DebugLog::log("[update] check failed: %s", snapshot.error.c_str());
        m_updateStatus = i18n.tr("dialog.update_check_failed",
                                 "Could not reach the update server.");
        publishUpdateState();
        if (m_updateCheckForced && m_settings)
            m_settings->requestToast(m_updateStatus, 3.0f);
        return;
    }
    if (snapshot.phase != update::UpdateClient::Phase::Available) {
        DebugLog::log("[update] already on the newest release");
        m_latestRelease = snapshot.release;
        m_pendingUpdate = {};
        m_updateStatus = i18n.tr("dialog.update_up_to_date", "SwitchU is up to date.");
        publishUpdateState();
        if (m_updateCheckForced && m_settings)
            m_settings->requestToast(m_updateStatus, 2.6f);
        return;
    }
    DebugLog::log("[update] %s available (%llu bytes)", snapshot.release.version.c_str(),
                  (unsigned long long)snapshot.release.sizeBytes);
    m_latestRelease = snapshot.release;
    m_pendingUpdate = snapshot.release;
    m_updateStatus = i18n.tr("dialog.update_available_short", "Update available")
                   + " (" + snapshot.release.version + ")";
    publishUpdateState();
    offerUpdate(snapshot.release);
#endif
}

void WiiUMenuApp::showReleaseNotes() {
#ifdef SWITCHU_MENU
    if (!m_dialog)
        return;
    auto& i18n = nxui::I18n::instance();
    const std::string language = i18n.activeLanguageTag();
    const bool havePublished = !m_latestRelease.notes.empty();
    const std::string notes = havePublished
        ? update::UpdateClient::condenseNotes(m_latestRelease.notes, language)
        : loadBundledReleaseNotes(language);
    if (notes.empty())
        return;
    m_audio.playSfx(Sfx::ModalShow);
    raiseOverlay(m_dialog);
    m_dialog->show(
        std::string("SwitchU ") + (havePublished ? m_latestRelease.version : SWITCHU_VERSION),
        notes,
        {{i18n.tr("button.ok", "OK"), [this]() {}, true}},
        0, {});
    focusManager().setFocus(m_dialog.get());
#endif
}

void WiiUMenuApp::offerUpdate(const update::UpdateClient::Release& release) {
#ifdef SWITCHU_MENU
    if (m_updateOffered || !m_dialog)
        return;
    // Never interrupt something the player is in the middle of. The offer is
    // dropped rather than queued: the next boot asks again.
    if (m_dialog->isActive() || (m_settings && m_settings->isActive())
        || (m_themeShop && m_themeShop->isActive()) || (m_gameDetails && m_gameDetails->isActive())
        || (m_gameGallery && m_gameGallery->isActive()) || (m_gameMods && m_gameMods->isActive())
        || (m_userSelect && m_userSelect->isActive()) || m_editMode)
        return;
    m_updateOffered = true;

    auto& i18n = nxui::I18n::instance();
    const std::string notes = update::UpdateClient::condenseNotes(
        release.notes, i18n.activeLanguageTag());
    std::string body = i18n.tr("dialog.update_body", "A new version of SwitchU is available.");
    body += "\n\n" + i18n.tr("dialog.update_installed", "Installed") + ": " + SWITCHU_VERSION;
    body += "\n" + i18n.tr("dialog.update_new", "New") + ": " + release.version;
    if (!notes.empty())
        body += "\n\n" + notes;

    m_audio.playSfx(Sfx::ModalShow);
    raiseOverlay(m_dialog);
    m_dialog->show(
        i18n.tr("dialog.update_title", "Update available"),
        body,
        {
            {i18n.tr("dialog.update_later", "Later"), [this]() {}, true},
            {i18n.tr("dialog.update_install", "Update"), [this, release]() {
                 startUpdateDownload(release);
             }, false},
        },
        0, {});
    focusManager().setFocus(m_dialog.get());
#else
    (void)release;
#endif
}

void WiiUMenuApp::startUpdateDownload(const update::UpdateClient::Release& release) {
#ifdef SWITCHU_MENU
    if (m_updateDownloadFuture.valid()
        && m_updateDownloadFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    auto shared = std::make_shared<UpdateDownload>();
    shared->release = release;
    shared->total = release.sizeBytes;
    m_updateDownload = shared;

    auto& i18n = nxui::I18n::instance();
    if (m_progressDialog) {
        m_progressDialog->setTheme(&m_theme);
        raiseOverlay(m_progressDialog);
        m_progressDialog->show(i18n.tr("dialog.update_title", "Update available"),
                               i18n.tr("dialog.update_downloading", "Downloading update..."),
                               -1.f);
        focusManager().setFocus(m_progressDialog.get());
    }

    m_updateDownloadFuture = m_threadPool.submit([shared]() {
        try {
            if (!ensureUpdateDirectory())
                throw std::runtime_error("could not create the update folder");
            std::remove(kUpdateArchive);
            const std::uint64_t written = themeshop::http::getToFile(
                shared->release.downloadUrl, kUpdateArchive,
                [shared](std::uint64_t received, std::uint64_t total) {
                    std::lock_guard<std::mutex> lock(shared->mutex);
                    shared->received = received;
                    if (total) shared->total = total;
                });

            // Only now, with the whole archive on the card and its length
            // agreeing with what the release published, is anything replaced.
            if (shared->release.sizeBytes && written != shared->release.sizeBytes)
                throw std::runtime_error("download size did not match the release");

            {
                std::lock_guard<std::mutex> lock(shared->mutex);
                shared->extracting = true;
            }
            const auto result = themeshop::extractZipFile(kUpdateArchive, kCardRoot);
            if (!result.success)
                throw std::runtime_error(result.error.empty() ? "could not unpack the update"
                                                              : result.error);
            std::remove(kUpdateArchive);

            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->ok = true;
            shared->done = true;
        } catch (const std::exception& error) {
            std::remove(kUpdateArchive);
            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->error = error.what();
            shared->done = true;
        } catch (...) {
            std::remove(kUpdateArchive);
            std::lock_guard<std::mutex> lock(shared->mutex);
            shared->error = "unknown update failure";
            shared->done = true;
        }
    });
#else
    (void)release;
#endif
}

void WiiUMenuApp::syncUpdateDownload() {
#ifdef SWITCHU_MENU
    auto shared = m_updateDownload;
    if (!shared)
        return;

    auto& i18n = nxui::I18n::instance();
    bool done = false, ok = false;
    std::string error;
    std::uint64_t received = 0, total = 0;
    bool extracting = false;
    {
        std::lock_guard<std::mutex> lock(shared->mutex);
        done = shared->done;
        ok = shared->ok;
        error = shared->error;
        received = shared->received;
        total = shared->total;
        extracting = shared->extracting;
    }

    if (!done) {
        if (m_progressDialog && m_progressDialog->isActive()) {
            if (extracting) {
                m_progressDialog->updateState(
                    i18n.tr("dialog.update_applying", "Applying update..."), -1.f);
            } else if (total > 0) {
                m_progressDialog->updateState(
                    i18n.tr("dialog.update_downloading", "Downloading update..."),
                    (float)received / (float)total);
            }
        }
        return;
    }

    m_updateDownload.reset();
    if (ok)
        m_pendingUpdate = {};   // installed: there is nothing left to offer
    m_updateStatus = nxui::I18n::instance().tr(
        ok ? "dialog.update_done" : "dialog.update_failed",
        ok ? "Update installed. Restart the console to run the new version."
           : "The update could not be installed.");
    // Releases the tab's busy state and shows the outcome there as well, not
    // only in the dialog the player is about to dismiss.
    publishUpdateState();
    if (m_progressDialog)
        m_progressDialog->hide();

    // The card holds the new files now; committing before the reboot keeps a
    // pulled power cable from leaving half of them behind.
    if (ok)
        quiesceWritersForPowerAction();

    m_audio.playSfx(Sfx::ModalShow);
    raiseOverlay(m_dialog);
    m_dialog->show(
        i18n.tr("dialog.update_title", "Update available"),
        ok ? i18n.tr("dialog.update_done",
                     "Update installed. Restart the console to run the new version.")
           : i18n.tr("dialog.update_failed", "The update could not be installed."),
        {{i18n.tr("button.ok", "OK"), [this]() {}, true}},
        0, {});
    focusManager().setFocus(m_dialog.get());
    DebugLog::log("[update] %s%s", ok ? "installed" : "failed: ", ok ? "" : error.c_str());
#endif
}
