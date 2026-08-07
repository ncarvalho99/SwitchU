#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <switch.h>

class AppletLauncher {
public:
    struct Callbacks {
        std::function<void()>     playSfxModalHide;
        std::function<void()>     requestExit;
        // Blocks until this process has no disk write outstanding. Power
        // requests call it before handing off, so the reboot cannot catch one
        // of our writes half-done.
        std::function<void()>     quiesceWriters;
    };

    void init(Callbacks cbs);

    void quiesceForPower(const char* what);

    void launchAlbum();
    void launchMiiEditor();
    void launchControllerPairing();
    void launchNetConnect();
    void launchUserPage(AccountUid uid);
    void launchUserCreator();
    // Writes the request the forked loader reads, then asks the daemon to
    // start whichever applet is hosting it. Returns false without launching
    // if the request could not be written -- starting anyway would open the
    // loader on hbmenu, which looks like the wrong homebrew ran.
    bool launchHomebrew(const std::string& nroPath, int appletId);
    void enterSleep();
    void shutdown();
    void reboot();

    void launchApplication(uint64_t titleId, AccountUid uid);
    void resumeApplication();
    void terminateApplication();

    void checkRunningApplication();

    bool     isAppRunning()  const;
    bool     isAppSuspended(uint64_t titleId) const;
    uint64_t suspendedTitleId() const;

    void setAppRunning(bool v);
    void setAppHasForeground(bool v);
    void setSuspendedTitleId(uint64_t v);

#ifdef SWITCHU_MENU
    void setStartupStatus(uint64_t suspendedTitleId, bool appRunning);
#endif

private:
#ifdef SWITCHU_MENU
    std::atomic<bool>     m_appRunning{false};
    std::atomic<bool>     m_appHasForeground{false};
    std::atomic<uint64_t> m_suspendedTitleId{0};
#endif

    Callbacks m_cb;
};
