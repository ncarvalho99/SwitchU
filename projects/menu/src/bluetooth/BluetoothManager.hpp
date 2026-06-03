#pragma once

#include <vector>
#include <cstring>
#include <string>
#include <switch.h>

namespace bluetooth {

void Initialize();
void Finalize();

bool IsAvailable();

std::vector<BtmAudioDevice> ListPairedAudioDevices();
bool HasPairedChanges();

BtmAudioDevice GetConnectedAudioDevice();
bool HasConnectedChanges();

std::vector<BtmAudioDevice> ListDiscoveredAudioDevices();
bool HasDiscoveredChanges();

Result ConnectAudioDevice(const BtmAudioDevice& device);
Result DisconnectAudioDevice(const BtmAudioDevice& device);
Result UnpairAudioDevice(const BtmAudioDevice& device);
std::string DeviceName(const BtmAudioDevice& device);

void StartDiscovery();
void StopDiscovery();
bool IsDiscovering();

inline bool AddressesEqual(const BtdrvAddress& a, const BtdrvAddress& b) {
    return memcmp(&a, &b, sizeof(BtdrvAddress)) == 0;
}

inline bool IsDeviceValid(const BtmAudioDevice& device) {
    static constexpr BtdrvAddress kInvalid = {0, 0, 0, 0, 0, 0};
    return memcmp(&device.addr, &kInvalid, sizeof(BtdrvAddress)) != 0;
}

}
