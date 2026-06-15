
#pragma once

#include "OmniInstances.h"

#include <cstdint>

// Helper function to generate and return the 8 dummy instances.
inline std::unordered_map<DeviceMap, OmniInstance> GenerateAvailableInstances()
{
    struct DummyConfig
    {
        DeviceMap key;
        const char* name;
        uint32_t ip;
        const char* ipStr;
    };

    DummyConfig configs[8] = {{C0, "Dev_Center_0", 0x1400A8C0, "192.168.0.20"},
                              {L1, "Dev_Left_1", 0x1500A8C0, "192.168.0.21"},
                              {U1, "Dev_Up_1", 0x1600A8C0, "192.168.0.22"},
                              {R1, "Dev_Right_1", 0x1700A8C0, "192.168.0.23"},
                              {D1, "Dev_Down_1", 0x1800A8C0, "192.168.0.24"},
                              {LU1, "Dev_LeftUp_1", 0x1900A8C0, "192.168.0.25"},
                              {RU1, "Dev_RightUp_1", 0x1A00A8C0, "192.168.0.26"},
                              {RD1, "Dev_RightDn_1", 0x1B00A8C0, "192.168.0.27"}};
    std::unordered_map<DeviceMap, OmniInstance> dummyMap;

    for (const auto& config : configs) {
        OmniInstance inst(static_cast<uint8_t>(config.key));

#if defined(_MSC_VER)
        strncpy_s(inst.InstanceName, config.name, OmniDevNameLen);
        strncpy_s(inst.IPv4_String, config.ipStr, 15);
#else
        std::strncpy(inst.InstanceName, config.name, OmniDevNameLen);
        std::strncpy(inst.IPv4_String, config.ipStr, 15);
#endif

        inst.InstanceIP = config.ip;

        dummyMap[config.key] = inst;
    }

    return dummyMap;
}

// Global registry for direct use across
inline std::unordered_map<DeviceMap, OmniInstance> DummyAvailableInstances =
    GenerateAvailableInstances();
