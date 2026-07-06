#pragma once
#include "InstanceRegistry.h"
#include <cassert>
#include <iostream>

// Custom derivation OmniInstanceRegistry for testing since OmniInstanceRegistry constructor tries
// to scan local network via new OmniDiscovery
struct MockInstanceRegistry : public OmniInstanceRegistry
{
    MockInstanceRegistry()
    {
        // bye..
        delete InstanceProbe;
        InstanceProbe = nullptr;

        // Dummies
        AllInstances[DeviceMap::C0].InstanceIP = 0x0100007F; // 127.0.0.1
        strcpy_s(AllInstances[DeviceMap::C0].IPv4_String, "127.0.0.1");
        strcpy_s(AllInstances[DeviceMap::C0].InstanceName, "TestUserDevice");
        AllInstances[DeviceMap::C0].DevMapIndex = static_cast<uint8_t>(DeviceMap::C0);

        // Clean state
        OpenSlotMask = 0x1FF;
        AllInstances.clear();
        InstanceLookup.clear();

        AllInstances[DeviceMap::C0].InstanceIP = 0x0100007F;
        strcpy_s(AllInstances[DeviceMap::C0].IPv4_String, "127.0.0.1");
        strcpy_s(AllInstances[DeviceMap::C0].InstanceName, "TestUserDevice");
        AllInstances[DeviceMap::C0].DevMapIndex = static_cast<uint8_t>(DeviceMap::C0);

        // Setting C0 asa occupied
        OpenSlotMask &= ~(1 << static_cast<uint8_t>(DeviceMap::C0));
    }

    void ForceAddInstance(uint32_t IP, DeviceMap slot) { AddInstance(IP, slot); }

    void ForceClearInstance(DeviceMap slot) { ClearInstance(slot); }
};

void InstanceRegistryTest()
{
    std::cout << "[RUN] InstanceRegistryTest\n";

    MockInstanceRegistry reg;

    // Available count must be 8 since i took C0 earlier
    assert(reg.GetAllInstancesCount() == 8);
    assert(reg.AllInstances[DeviceMap::C0].InstanceIP == 0x0100007F);

    reg.ForceAddInstance(0x0200A8C0, DeviceMap::L1);
    assert(reg.AllInstances[DeviceMap::L1].InstanceIP == 0x0200A8C0);
    assert(reg.InstanceLookup[0x0200A8C0] == DeviceMap::L1);
    assert(reg.GetAllInstancesCount() == 7);

    // Slot swapping
    reg.SwapInstances(DeviceMap::L1, DeviceMap::R1);
    assert(reg.AllInstances[DeviceMap::L1].InstanceIP == 0);
    assert(reg.AllInstances[DeviceMap::R1].InstanceIP == 0x0200A8C0);

    // Clear..
    reg.ForceClearInstance(DeviceMap::R1);
    assert(reg.AllInstances[DeviceMap::R1].InstanceIP == 0);

    std::cout << "[PASS] InstanceRegistryTest\n";
}
