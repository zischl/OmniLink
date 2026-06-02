#ifndef OmniInstanceReg_H
#define OmniInstanceReg_H

#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

#include "NetUtils.h"
#include "OmniDiscovery.h"
#include "OmniEnums.h"
#include "OmniInstances.h"
#include "OmniLogger.h"
#include "system_probe_impl.h"

#include <cstdint>

struct InstanceRegistry
{
  protected:
    std::mutex Mutex;
    Instances* InstanceProbe = nullptr;
    uint32_t OpenSlotMask = 0x1FF;

  public:
    std::unordered_map<DeviceMap, OmniInstance> AllInstances = {{DeviceMap::LU1, OmniInstance(5)},
                                                                {DeviceMap::U1, OmniInstance(2)},
                                                                {DeviceMap::RU1, OmniInstance(6)},
                                                                {DeviceMap::L1, OmniInstance(1)},
                                                                {DeviceMap::C0, OmniInstance(0)},
                                                                {DeviceMap::R1, OmniInstance(3)},
                                                                {DeviceMap::LD1, OmniInstance(8)},
                                                                {DeviceMap::D1, OmniInstance(4)},
                                                                {DeviceMap::RD1, OmniInstance(7)}};

    ActiveInstanceContainer ActiveInstances;

    std::unordered_map<uint32_t, DeviceMap> InstanceLookup = {};

    InstanceRegistry()
    {
        // Getting user data and initializing user instance
        uint32_t LocalIP;
        Device::RetrieveLocalIP(LocalIP, 0);
        if (LocalIP != 0) {
            IP2Char(LocalIP, ActiveInstances[DeviceMap::C0].IPv4_String);
        }

        // Getting user data and initializing user instance
        Device::RetrieveUserName(ActiveInstances[DeviceMap::C0].InstanceName);
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
    // Otherwise initiate a new scana, uses bit masking to store open slots
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

    void SwapInstances(int DeviceID1, int DeviceID2)
    {
        if (AllInstances[DeviceMap(DeviceID2)].InstanceIP == NULL) {
            AllInstances[DeviceMap(DeviceID2)].InstanceIP =
                AllInstances[DeviceMap(DeviceID1)].InstanceIP;
            strncpy(AllInstances[DeviceMap(DeviceID2)].InstanceName,
                    AllInstances[DeviceMap(DeviceID1)].InstanceName,
                    OmniDevNameLen);
            strncpy(AllInstances[DeviceMap(DeviceID2)].IPv4_String,
                    AllInstances[DeviceMap(DeviceID1)].IPv4_String,
                    16);

            AllInstances[DeviceMap(DeviceID1)].Clear();

        }

        else {
            OmniInstance temp = AllInstances[DeviceMap(DeviceID1)];
            AllInstances[DeviceMap(DeviceID1)].Edit(AllInstances[DeviceMap(DeviceID2)].InstanceName,
                                                    AllInstances[DeviceMap(DeviceID2)].IPv4_String,
                                                    AllInstances[DeviceMap(DeviceID2)].InstanceIP);
            AllInstances[DeviceMap(DeviceID2)].Edit(
                temp.InstanceName, temp.IPv4_String, temp.InstanceIP);
        }
    }
};

#endif
