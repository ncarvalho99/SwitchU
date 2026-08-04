#include "AppletLauncher.hpp"
#include "core/DebugLog.hpp"
#ifdef SWITCHU_MENU
#include "smi_commands.hpp"
#include <switchu/smi_protocol.hpp>
#include <switchu/sd_commit.hpp>
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

void AppletLauncher::launchUserPage(AccountUid uid) {
    DebugLog::log("[launcher] requesting User Page launch via daemon");
    Result rc = switchu::menu::smi_cmd::launchUserPage(uid);
    DebugLog::log("[launcher] User Page rc=0x%X", rc);
    if (R_SUCCEEDED(rc)) {
        if (m_cb.playSfxModalHide) m_cb.playSfxModalHide();
        if (m_cb.requestExit)      m_cb.requestExit();
    }
}

// The daemon commits the SD card before starting a power sequence, but a
// commit only flushes the calling process's own fs session. This menu writes
// config.json and layout.json — a settings change queues one moments before
// the user picks Reboot — and those writes live in a different session
// entirely. The daemon then reboots without waiting for this applet to exit,
// so they were still dirty when power dropped. That is the SD corruption that
// kept returning after a reboot from the power menu.
static void commitBeforePowerAction(const char* what) {
    DebugLog::log("[launcher] requesting %s, committing sd first", what);
    switchu::FileLog::flush();
    switchu::commitSdCard("menu-power-request");
}

void AppletLauncher::enterSleep() {
    commitBeforePowerAction("sleep");
    switchu::menu::smi_cmd::enterSleep();
}

void AppletLauncher::shutdown() {
    commitBeforePowerAction("shutdown");
    switchu::menu::smi_cmd::shutdown();
}

void AppletLauncher::reboot() {
    commitBeforePowerAction("reboot");
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
