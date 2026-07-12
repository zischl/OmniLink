#ifndef OMNIINSTANCES_H
#define OMNIINSTANCES_H

#pragma once
#include "OmniConfig.h"
#include "OmniEnums.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#endif

#define OmniDevNameLen 31

template <uint32_t MTU> class OmniNetSession;

struct OmniIP
{
    uint32_t InstanceIP = 0;
    char IPv4_String[32] = {};
};

struct OmniInstance
{
    char InstanceName[OmniDevNameLen + 1] = {};
    uint32_t InstanceIP = 0;
    char IPv4_String[16] = {};
    uint8_t DevMapIndex = 0;
    NetLinkState LinkState = NetLinkState::INACTIVE;
    uint32_t HandshakeToken = 0;

    OmniInstance() {}

    OmniInstance(uint8_t DevMIndex) { DevMapIndex = DevMIndex; }

    void Clear()
    {
        memset(InstanceName, 0, sizeof(InstanceName));
        InstanceIP = 0;
        memset(IPv4_String, 0, sizeof(IPv4_String));
        LinkState = NetLinkState::INACTIVE;
        HandshakeToken = 0;
    }

    void Edit(char* InstanceName_, char* IPv4_String_, uint32_t InstanceIP_, DeviceMap DeviceID)
    {
        InstanceIP = InstanceIP_;
        strncpy(IPv4_String, IPv4_String_, 16);
        strncpy(InstanceName, InstanceName_, (OmniDevNameLen + 1));
        DevMapIndex = static_cast<uint8_t>(DevMapIndex);
    }
};

struct OmniActiveInstance : OmniInstance
{
    std::unique_ptr<OmniNetSession<OmniMTU>> InstanceSession;
    uint16_t port = 62485;
    uint32_t ActiveFlags = FeatureFlags::fInactive;

    OmniActiveInstance() {}

    OmniActiveInstance(
        char* InstanceName_, char* IPv4_String_, uint32_t InstanceIP_, uint8_t DeviceID
    )
    {
        InstanceIP = InstanceIP_;
        strncpy(IPv4_String, IPv4_String_, 16);
        strncpy(InstanceName, InstanceName_, (OmniDevNameLen + 1));
        DevMapIndex = DeviceID;
    }

    OmniActiveInstance(OmniInstance& Instance)
    {
        InstanceIP = Instance.InstanceIP;
        strncpy(IPv4_String, Instance.IPv4_String, 16);
        strncpy(InstanceName, Instance.InstanceName, (OmniDevNameLen + 1));
        DevMapIndex = Instance.DevMapIndex;
    }

    inline void ToggleFeatureState(FeatureFlags Feature) { ActiveFlags ^= Feature; }

    inline bool GetFeatureState(FeatureFlags Feature) const { return (ActiveFlags & Feature) != 0; }
};

using ActiveInstanceContainer = std::unordered_map<DeviceMap, OmniActiveInstance>;

template <size_t MaxFrameLen> struct FrameByte
{
    char Frame[MaxFrameLen];
    size_t FrameLen = 0;

    FrameByte(size_t frame_len) { FrameLen = frame_len; }
};

#endif // OMNIINSTANCES_H
