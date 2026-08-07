#include "AppletLauncher.hpp"
#include <system_error>
#include <fstream>
#include <filesystem>
#include "core/DebugLog.hpp"
#include <switchu/sd_commit.hpp>
#ifdef SWITCHU_MENU
#include "smi_commands.hpp"
#include <switchu/smi_protocol.hpp>
#endif
#include <switch.h>

void AppletLauncher::init(Callbacks cbs) {
    m_cb = std::move(cbs);
}

#ifdef SWITCHU_MENU
bool AppletLauncher::isAppRunning() const  { return m_appRunning; }
bool AppletLauncher::isAppSuspended(uint64_t titleId) const {
    return m_suspendedTitleId != 0 && m_suspendedTitleId == titleId;
}
uint64_t AppletLauncher::suspendedTitleId() const { return m_suspendedTitleId; }

void AppletLauncher::setAppRunning(bool v)          { m_appRunning = v; }
void AppletLauncher::setAppHasForeground(bool v)    { m_appHasForeground = v; }
void AppletLauncher::setSuspendedTitleId(uint64_t v){ m_suspendedTitleId = v; }

void AppletLauncher::setStartupStatus(uint64_t suspendedTitleId, bool appRunning) {
    m_suspendedTitleId = suspendedTitleId;
    m_appRunning       = appRunning;
    m_appHasForeground = false;
    DebugLog::log("[launcher] startup status: suspended=0x%016lX running=%d",
                  suspendedTitleId, appRunning);
}

void AppletLauncher::launchAlbum() {
    DebugLog::log("[launcher] requesting Album launch via daemon");
    Result rc = switchu::menu::smi_cmd::sendSimple(switchu::smi::SystemMessage::LaunchAlbum);
    DebugLog::log("[launcher] Album rc=0x%X", rc);
    if (R_SUCCEEDED(rc)) {
        if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
        if (m_cb.requestExit)      m_cb.requestExit();
    }
}

bool AppletLauncher::launchHomebrew(const std::string& nroPath, int appletId) {
    // The loader reads this once and deletes it. Writing it before the applet
    // starts is the whole handshake -- see lib/switchu-hbloader.
    {
        std::error_code ec;
        std::filesystem::create_directories("sdmc:/config/SwitchU", ec);
        std::ofstream out("sdmc:/config/SwitchU/next_nro.txt",
                          std::ios::binary | std::ios::trunc);
        out << nroPath << "\n";
        out.flush();
        if (!out) {
            DebugLog::log("[launcher] could not write the homebrew request");
            return false;
        }
    }

    // The applet starts and this process ends moments later, so the request
    // must be on the card rather than in a cache by the time that happens.
    switchu::commitSdCard("homebrew request");

    DebugLog::log("[launcher] homebrew %s via applet 0x%X", nroPath.c_str(), appletId);
    Result rc = switchu::menu::smi_cmd::launchHomebrew((uint32_t)appletId);
    DebugLog::log("[launcher] homebrew rc=0x%X", rc);
    if (R_FAILED(rc)) {
        // Leaving the request behind would make the next launch open the wrong
        // thing, so it goes with the failure that produced it.
        std::error_code ec;
        std::filesystem::remove("sdmc:/config/SwitchU/next_nro.txt", ec);
        return false;
    }

    if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
    if (m_cb.requestExit)      m_cb.requestExit();
    return true;
}
void AppletLauncher::launchMiiEditor() {
    DebugLog::log("[launcher] requesting Mii Editor launch via daemon");
    Result rc = switchu::menu::smi_cmd::sendSimple(switchu::smi::SystemMessage::LaunchMiiEditor);
    DebugLog::log("[launcher] Mii Editor rc=0x%X", rc);
    if (R_SUCCEEDED(rc)) {
        if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
        if (m_cb.requestExit)      m_cb.requestExit();
    }
}

void AppletLauncher::launchControllerPairing() {
    DebugLog::log("[launcher] requesting Controller pairing via daemon");
    Result rc = switchu::menu::smi_cmd::sendSimple(switchu::smi::SystemMessage::LaunchControllers);
    DebugLog::log("[launcher] Controller pairing rc=0x%X", rc);
    if (R_SUCCEEDED(rc)) {
        if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
        if (m_cb.requestExit)      m_cb.requestExit();
    }
}

void AppletLauncher::launchNetConnect() {
    DebugLog::log("[launcher] requesting NetConnect launch via daemon");
    Result rc = switchu::menu::smi_cmd::sendSimple(switchu::smi::SystemMessage::LaunchNetConnect);
    DebugLog::log("[launcher] NetConnect rc=0x%X", rc);
    if (R_SUCCEEDED(rc)) {
        if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
        if (m_cb.requestExit)      m_cb.requestExit();
    }
}

void AppletLauncher::launchUserCreator() {
    DebugLog::log("[launcher] requesting user creator via daemon");
    Result rc = switchu::menu::smi_cmd::sendSimple(switchu::smi::SystemMessage::LaunchUserCreator);
    DebugLog::log("[launcher] user creator rc=0x%X", rc);
    if (R_SUCCEEDED(rc)) {
        if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
        if (m_cb.requestExit)      m_cb.requestExit();
    }
}

void AppletLauncher::launchUserPage(AccountUid uid) {
    DebugLog::log("[launcher] requesting User Page launch via daemon");
    Result rc = switchu::menu::smi_cmd::launchUserPage(uid);
    DebugLog::log("[launcher] User Page rc=0x%X", rc);
    if (R_SUCCEEDED(rc)) {
        if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
        if (m_cb.requestExit)      m_cb.requestExit();
    }
}

// Wait for our own writes before the daemon is told to cut power. The menu
// does not exit here: asking it to made the daemon see no menu running and
// relaunch it into the middle of the reboot, leaving the console black.
void AppletLauncher::quiesceForPower(const char* what) {
    if (m_cb.quiesceWriters)
        m_cb.quiesceWriters();
    DebugLog::log("[launcher] requesting %s, writers quiesced", what);
}

void AppletLauncher::enterSleep() {
    quiesceForPower("sleep");
    switchu::menu::smi_cmd::enterSleep();
}

void AppletLauncher::shutdown() {
    quiesceForPower("shutdown");
    switchu::menu::smi_cmd::shutdown();
}

void AppletLauncher::reboot() {
    quiesceForPower("reboot");
    switchu::menu::smi_cmd::reboot();
}

void AppletLauncher::launchApplication(uint64_t titleId, AccountUid uid) {
    DebugLog::log("[launcher] tid=%016lX", titleId);
    Result rc = switchu::menu::smi_cmd::launchApplication(titleId, uid);
    if (R_FAILED(rc)) {
        DebugLog::log("[launcher] FAIL: 0x%X", rc);
        return;
    }
    DebugLog::log("[launcher] command sent, closing menu");
    if (m_cb.requestExit) m_cb.requestExit();
}

void AppletLauncher::resumeApplication() {
    if (m_suspendedTitleId == 0) {
        DebugLog::log("[launcher] no app suspended!");
        return;
    }
    DebugLog::log("[launcher] resume, closing menu");
    switchu::menu::smi_cmd::resumeApplication();
    if (m_cb.requestExit) m_cb.requestExit();
}

void AppletLauncher::terminateApplication() {
    if (m_suspendedTitleId == 0) {
        DebugLog::log("[launcher] no app suspended, nothing to terminate");
        return;
    }
    DebugLog::log("[launcher] requesting terminate 0x%016lX", (uint64_t)m_suspendedTitleId);
    switchu::menu::smi_cmd::terminateApplication();
}

void AppletLauncher::checkRunningApplication() {
}

#else

bool AppletLauncher::isAppRunning() const            { return false; }
bool AppletLauncher::isAppSuspended(uint64_t) const  { return false; }
uint64_t AppletLauncher::suspendedTitleId() const    { return 0; }
void AppletLauncher::setAppRunning(bool)             {}
void AppletLauncher::setAppHasForeground(bool)       {}
void AppletLauncher::setSuspendedTitleId(uint64_t)   {}

void AppletLauncher::launchAlbum()             {}
void AppletLauncher::launchMiiEditor()         {}
void AppletLauncher::launchUserCreator()       {}
bool AppletLauncher::launchHomebrew(const std::string&, int) { return false; }
void AppletLauncher::launchControllerPairing() {}
void AppletLauncher::launchNetConnect()        {}
void AppletLauncher::launchUserPage(AccountUid) {}
void AppletLauncher::enterSleep()              {}
void AppletLauncher::shutdown()                {}
void AppletLauncher::reboot()                  {}
void AppletLauncher::launchApplication(uint64_t, AccountUid) {}
void AppletLauncher::resumeApplication()       {}
void AppletLauncher::terminateApplication()    {}
void AppletLauncher::checkRunningApplication() {}

#endif
