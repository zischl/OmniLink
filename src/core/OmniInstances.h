#ifndef OMNIINSTANCES_H
#define OMNIINSTANCES_H

#pragma once
#include "OmniConfig.h"
#include "OmniEnums.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#endif

#define OmniDevNameLen 31

template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

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

inline FeatureFlags FeatureTypeToFlag(FeatureTypes Feature)
{
    return static_cast<FeatureFlags>(1 << static_cast<uint8_t>(Feature));
}

struct SubStreamEntry
{
    OmniNetSubStream* SubStream = nullptr;
    SubStreamState State = SubStreamState::Idle;
    uint16_t CaptureStreamID = 0;
};

struct OmniActiveInstance : OmniInstance
{
    std::unique_ptr<OmniNetSession<OmniMTU>> InstanceSession;
    uint16_t port = 62485;
    uint32_t OutboundFlags = FeatureFlags::fInactive;
    uint32_t InboundFlags = FeatureFlags::fInactive;
    uint32_t ActiveFlags = FeatureFlags::fInactive;

    std::unordered_map<uint16_t, SubStreamEntry> SubStreamRegistry;
    static inline std::atomic<uint16_t> NextSubStreamID{1};

    SubStreamEntry* FindSubStream(uint16_t ID)
    {
        auto iter = SubStreamRegistry.find(ID);
        return (iter != SubStreamRegistry.end()) ? &iter->second : nullptr;
    }

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

    inline void SetFeatureState(FeatureTypes Feature, FeatureActionRoute Route, bool State)
    {
        uint32_t Flag = 1 << static_cast<uint32_t>(Feature);
        uint32_t& TargetFlags =
            (Route == FeatureActionRoute::Outbound) ? OutboundFlags : InboundFlags;

        if (State)
            TargetFlags |= Flag;
        else
            TargetFlags &= ~Flag;

        ActiveFlags = OutboundFlags | InboundFlags;
    }

    inline bool GetFeatureState(FeatureTypes Feature, FeatureActionRoute Route) const
    {
        uint32_t Flag = 1 << static_cast<uint32_t>(Feature);
        uint32_t Flags = (Route == FeatureActionRoute::Outbound) ? OutboundFlags : InboundFlags;
        return (Flags & Flag) != 0;
    }

    inline bool GetFeatureState(FeatureFlags Feature) const { return (ActiveFlags & Feature) != 0; }

    inline FeatureLinkState GetLinkState(FeatureTypes Feature) const
    {
        uint32_t Flag = 1 << static_cast<uint32_t>(Feature);
        uint8_t State = 0;
        if (OutboundFlags & Flag)
            State |= static_cast<uint8_t>(FeatureLinkState::OutboundOnly);
        if (InboundFlags & Flag)
            State |= static_cast<uint8_t>(FeatureLinkState::InboundOnly);
        return static_cast<FeatureLinkState>(State);
    }
};

using ActiveInstanceContainer = std::unordered_map<DeviceMap, OmniActiveInstance>;

template <size_t MaxFrameLen> struct FrameByte
{
    char Frame[MaxFrameLen];
    size_t FrameLen = 0;

    FrameByte(size_t frame_len) { FrameLen = frame_len; }
};

#endif // OMNIINSTANCES_H
