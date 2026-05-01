#include "SystemMessages.hpp"
#include "DebugLog.hpp"
#include <switch.h>

#ifdef SWITCHU_MENU

void SystemMessages::pushAction(SysAction a) {
    std::lock_guard<std::mutex> lk(m_mutex);
    m_pending.push_back(a);
}

void SystemMessages::pump() {
    std::vector<SysAction> actions;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        actions.swap(m_pending);
    }
    if (m_callback) {
        for (auto a : actions)
            m_callback(a);
    }
}

void SystemMessages::start() {
}

void SystemMessages::stop() {
}
#endif
