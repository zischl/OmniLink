#ifndef OmniInstanceReg_H
#define OmniInstanceReg_H

#pragma once
#include "NetUtils.h"
#include "OmniDiscovery.h"
#include "OmniInstances.h"
#include "system_probe_impl.h"

struct InstanceRegistry
{
    std::mutex Mutex;
    Instances* InstanceProbe = nullptr;

    std::unordered_map<DeviceMap, OmniInstance> AllInstances = {{DeviceMap::LU1, OmniInstance(5)},
                                                                {DeviceMap::U1, OmniInstance(2)},
                                                                {DeviceMap::RU1, OmniInstance(6)},
                                                                {DeviceMap::L1, OmniInstance(1)},
                                                                {DeviceMap::C0, OmniInstance(0)},
                                                                {DeviceMap::R1, OmniInstance(3)},
                                                                {DeviceMap::LD1, OmniInstance(8)},
                                                                {DeviceMap::D1, OmniInstance(4)},
                                                                {DeviceMap::RD1, OmniInstance(7)}};

    std::unordered_map<DeviceMap, OmniActiveInstance> ActiveInstances;

    std::unordered_map<uint32_t, DeviceMap> InstanceLookup = {};

    InstanceRegistry()
    {
        // Getting user data and initializing user instance
        uint32_t LocalIP;
        Device::RetrieveLocalIP(LocalIP, 0);
        if (LocalIP != 0) {
            IP2Char(LocalIP, ActiveInstances[DeviceMap::C0].IPv4_String);
        }
    }

    std::unordered_map<DeviceMap, OmniInstance>* GetAvailableInstances() noexcept;

    // Check whether new scan results are available and get them if so
    // Otherwise initiate a new scan
    template <typename DiscoveryCallback> void ScanInstances(DiscoveryCallback* Callback)
    {
        if (InstanceProbe->ScanState.load()) {
            std::unordered_map<uint32_t, std::string> AvailableInstances = *InstanceProbe->get();

            int DevIdx = 0;

            // AllInstances.clear();

            for (const auto& [IP, Name] : AvailableInstances) {
                if (InstanceLookup.contains(IP))
                    continue;

                std::lock_guard<std::mutex> lock(Mutex);
                AllInstances[static_cast<DeviceMap>(DevIdx)].InstanceIP = IP;
                std::sprintf(AllInstances[static_cast<DeviceMap>(DevIdx)].IPv4_String,
                             "%u.%u.%u.%u",
                             (IP >> 24) & 0xFF,
                             (IP >> 16) & 0xFF,
                             (IP >> 8) & 0xFF,
                             IP & 0xFF);
                DevIdx++;
            }

            for (auto& [DevMapIDx, Instance] : AllInstances) {
                if (AvailableInstances.find(Instance.InstanceIP) == AvailableInstances.end()) {
                    Instance.Clear();
                }
            }

            DiscoveryCallback();
        } else {
            InstanceProbe->Scan(15);
        }
    }
};

#endif // !OmniInstanceReg_H
