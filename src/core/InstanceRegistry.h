#ifndef OmniInstanceReg_H
#define OmniInstanceReg_H

#pragma once
#include "NetUtils.h"
#include "OmniDiscovery.h"
#include "OmniEnums.h"
#include "OmniInstances.h"
#include "OmniLogger.h"
#include "system_probe_impl.h"

#include <cstdint>

struct InstanceRegistry
{
    std::mutex Mutex;
    Instances* InstanceProbe = nullptr;
    uint32_t OpenSlotMask = 0x1FF;

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

    std::unordered_map<DeviceMap, OmniInstance>* GetAvailableInstances() noexcept
    {
        return &AllInstances;
    }

    inline void ClearInstance(DeviceMap slot)
    {
        AllInstances[slot].Clear();
        OpenSlotMask |= (1 << static_cast<uint8_t>(slot));
    }

    // Check whether new scan results are available and get them if so
    // Otherwise initiate a new scan
    template <typename DiscoveryCallback> void RefreshInstanceList(DiscoveryCallback&& Callback)
    {
        if (!InstanceProbe->ScanState.load()) {
            InstanceProbe->Scan(15);
            return;
        }

        std::unordered_map<uint32_t, std::string> AvailableInstances = *InstanceProbe->get();

        std::lock_guard<std::mutex> lock(Mutex);

        for (auto& [DevMapIdx, Instance] : AllInstances) {
            if (Instance.InstanceIP != 0 && !AvailableInstances.contains(Instance.InstanceIP)) {
                ClearInstance(DevMapIdx);
            }
        }

        for (const auto& [IP, Name] : AvailableInstances) {
            if (InstanceLookup.contains(IP)) {
                continue;
            }

            if (OpenSlotMask == 0) {
                Logger::log("Why u trynna link 9 computers at home..");
                break;
            }

            unsigned long BitIndex;
            _BitScanForward(&BitIndex, OpenSlotMask);
            DeviceMap OpenSlot = static_cast<DeviceMap>(BitIndex);

            AllInstances[OpenSlot].InstanceIP = IP;
            std::sprintf(AllInstances[OpenSlot].IPv4_String,
                         "%u.%u.%u.%u",
                         (IP >> 24) & 0xFF,
                         (IP >> 16) & 0xFF,
                         (IP >> 8) & 0xFF,
                         IP & 0xFF);

            InstanceLookup[IP] = OpenSlot;

            OpenSlotMask &= ~(1 << BitIndex);
        }

        Callback();
    }
};

#endif // !OmniInstanceReg_H
