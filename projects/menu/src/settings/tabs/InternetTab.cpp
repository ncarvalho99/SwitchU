#include "TabBuilders.hpp"
#include "core/DebugLog.hpp"
#include <nxui/core/I18n.hpp>
#include <switch.h>
#include <cstdio>
#include <cstring>

static std::string settings_ipToString(u32 addr) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
                  addr & 0xFF, (addr >> 8) & 0xFF,
                  (addr >> 16) & 0xFF, (addr >> 24) & 0xFF);
    return buf;
}

static std::string settings_macToString(const u8* mac) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

// ThemeHttp keeps libnx's global nifm wrapper open as nifm:u.  The wrapper
// cannot be promoted to nifm:s while that session exists, so Airplane Mode
// must own a short-lived system session instead of using nifmInitialize().
class SettingsNifmSystemSession {
public:
    ~SettingsNifmSystemSession() {
        serviceClose(&m_general);
        serviceClose(&m_static);
    }

    Result open() {
        Result rc = smGetService(&m_static, "nifm:s");
        if (R_SUCCEEDED(rc))
            rc = serviceConvertToDomain(&m_static);
        if (R_SUCCEEDED(rc)) {
            const u64 reserved = 0;
            serviceAssumeDomain(&m_static);
            rc = serviceDispatchIn(&m_static, 5, reserved,
                .in_send_pid = true,
                .out_num_objects = 1,
                .out_objects = &m_general,
            );
        }
        return rc;
    }

    Result getWirelessCommunicationEnabled(bool* out) {
        u8 value = 0;
        serviceAssumeDomain(&m_general);
        Result rc = serviceDispatchOut(&m_general, 17, value);
        if (R_SUCCEEDED(rc) && out)
            *out = (value & 1) != 0;
        return rc;
    }

    Result setWirelessCommunicationEnabled(bool enabled) {
        const u8 value = enabled ? 1 : 0;
        serviceAssumeDomain(&m_general);
        return serviceDispatchIn(&m_general, 16, value);
    }

private:
    Service m_static{};
    Service m_general{};
};

static Result settings_getWirelessCommunicationEnabled(bool* out) {
    SettingsNifmSystemSession session;
    Result rc = session.open();
    if (R_SUCCEEDED(rc))
        rc = session.getWirelessCommunicationEnabled(out);
    return rc;
}

static Result settings_setWirelessCommunicationEnabled(bool enabled) {
    SettingsNifmSystemSession session;
    Result rc = session.open();
    if (R_SUCCEEDED(rc))
        rc = session.setWirelessCommunicationEnabled(enabled);
    return rc;
}

SettingsScreen::Tab settings::tabs::InternetTab::build(SettingsScreen& screen) {
    using Tab = SettingsScreen::Tab;
    using SettingItem = SettingsScreen::SettingItem;
    using ItemType = SettingsScreen::ItemType;
    auto& i18n = nxui::I18n::instance();
    Tab t;
    t.name = i18n.tr("settings.tabs.internet", "Internet");

    u32 ip = 0;
    std::string ssid;
    bool nifmOk = R_SUCCEEDED(nifmInitialize(NifmServiceType_User));
    DebugLog::log("[internet] nifmInit: %s", nifmOk ? "OK" : "FAIL");
    if (nifmOk) {
        nifmGetCurrentIpAddress(&ip);

        NifmNetworkProfileData prof{};
        if (R_SUCCEEDED(nifmGetCurrentNetworkProfile(&prof))) {
            char ssidBuf[33]{};
            std::memcpy(ssidBuf, prof.wireless_setting_data.ssid, 32);
            ssid = ssidBuf;
        }

        nifmExit();
    }

    bool wirelessCommunicationEnabled = true;
    Result wirelessCommunicationRc = settings_getWirelessCommunicationEnabled(&wirelessCommunicationEnabled);
    if (R_FAILED(wirelessCommunicationRc)) {
        DebugLog::log("[internet] airplane mode query failed: 0x%X", wirelessCommunicationRc);
    }

    {
        SettingItem it; it.label = i18n.tr("settings.internet.wifi_ssid", "WiFi Network"); it.type = ItemType::Action;
        it.description = !ssid.empty() ? ssid : i18n.tr("settings.internet.not_connected", "Not connected");
        it.onChange = [&screen](SettingItem&) {
            if (screen.m_netConnectCb) screen.m_netConnectCb();
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.internet.ip_address", "IP Address"); it.type = ItemType::Info;
        if (nifmOk && ip != 0)
            it.infoText = settings_ipToString(ip);
        else
            it.infoText = i18n.tr("settings.internet.not_connected", "Not connected");
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.internet.wifi", "WiFi"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.internet.wifi_desc", "Enable or disable the wireless LAN radio.");
        bool val = true;
        setsysGetWirelessLanEnableFlag(&val);
        it.boolVal = val && (R_FAILED(wirelessCommunicationRc) || wirelessCommunicationEnabled);
        it.anim01 = it.boolVal ? 1.f : 0.f;
        it.onChange = [](SettingItem& self) {
            bool wirelessEnabled = true;
            Result rc = settings_getWirelessCommunicationEnabled(&wirelessEnabled);
            if (R_SUCCEEDED(rc) && !wirelessEnabled && self.boolVal) {
                // Do not show a usable WiFi radio while Airplane Mode blocks
                // every wireless interface.
                self.boolVal = false;
                DebugLog::log("[internet] WiFi enable refused while airplane mode is on");
                return;
            }
            if (R_SUCCEEDED(rc))
                rc = setsysSetWirelessLanEnableFlag(self.boolVal);
            if (R_FAILED(rc)) {
                self.boolVal = !self.boolVal;
                DebugLog::log("[internet] WiFi change failed: 0x%X", rc);
            }
        };
        t.items.push_back(std::move(it));
    }

    {
        // nifm's global wireless-communication switch is the same system
        // state exposed as Airplane Mode: it disables every wireless radio,
        // unlike the WLAN-only setting above.
        SettingItem it; it.label = i18n.tr("settings.internet.airplane_mode", "Airplane Mode"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.internet.airplane_mode_desc", "Disable or enable all wireless communication, including WiFi, Bluetooth, and NFC.");

        it.boolVal = R_SUCCEEDED(wirelessCommunicationRc) && !wirelessCommunicationEnabled;
        it.anim01 = it.boolVal ? 1.f : 0.f;
        const std::string wifiLabel = i18n.tr("settings.internet.wifi", "WiFi");
        it.onChange = [&screen, wifiLabel](SettingItem& self) {
            // The control represents Airplane Mode, whereas libnx takes the
            // inverse: whether global wireless communication is enabled.
            Result rc = settings_setWirelessCommunicationEnabled(!self.boolVal);
            DebugLog::log("[internet] airplane mode -> %s: 0x%X",
                          self.boolVal ? "on" : "off", rc);
            if (R_FAILED(rc)) {
                // Do not leave the UI claiming a state that Horizon rejected.
                self.boolVal = !self.boolVal;
                return;
            }

            if (self.boolVal) {
                // Airplane Mode must visibly and actually disable the radio
                // switches exposed elsewhere in SwitchU, rather than merely
                // block their traffic at the NIFM layer.
                const Result wifiRc = setsysSetWirelessLanEnableFlag(false);
                const Result bluetoothRc = setsysSetBluetoothEnableFlag(false);
                const Result nfcRc = setsysSetNfcEnableFlag(false);
                DebugLog::log("[internet] airplane dependent radios wifi=0x%X bluetooth=0x%X nfc=0x%X",
                              wifiRc, bluetoothRc, nfcRc);
                screen.setCachedToggleState(wifiLabel, false);
            }
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.internet.auto_app_download", "Auto App Download"); it.type = ItemType::Toggle;
        it.description = i18n.tr("settings.internet.auto_app_download_desc", "Automatically download updates for installed games.");
        bool val = true;
        setsysGetAutomaticApplicationDownloadFlag(&val);
        it.boolVal = val;
        it.anim01 = val ? 1.f : 0.f;
        it.onChange = [](SettingItem& self) {
            setsysSetAutomaticApplicationDownloadFlag(self.boolVal);
        };
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.internet.mac_address", "MAC Address"); it.type = ItemType::Info;
        SetCalMacAddress mac{};
        if (R_SUCCEEDED(setcalInitialize())) {
            if (R_SUCCEEDED(setcalGetWirelessLanMacAddress(&mac)))
                it.infoText = settings_macToString(mac.addr);
            setcalExit();
        }
        if (it.infoText.empty())
            it.infoText = i18n.tr("common.na", "N/A");
        t.items.push_back(std::move(it));
    }

    {
        SettingItem it; it.label = i18n.tr("settings.internet.dns", "DNS"); it.type = ItemType::Info;
        it.infoText = i18n.tr("settings.internet.dns_auto", "Auto (DHCP)");
        t.items.push_back(std::move(it));
    }

    return t;
}
