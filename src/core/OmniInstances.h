#ifndef OMNIINSTANCES_H
#define OMNIINSTANCES_H

#pragma once
#include "OmniConfig.hpp"
#include "OmniEnums.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <optional>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#endif

#define OmniDevNameLen 31

template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;
class OmniTCPStream;

// Base OmniInstance Struct for holding Instance Data used in InstanceRegistry for mangement,
// Includes Handshake data for instance wise action protection.
// Includes Resolution data for scaling and mirroring purposes.
struct OmniInstance
{
    char         InstanceName[OmniDevNameLen + 1] = {};
    uint32_t     InstanceIP                       = 0;
    char         IPv4_String[16]                  = {};
    uint8_t      DevMapIndex                      = 0;
    NetLinkState LinkState                        = NetLinkState::INACTIVE;
    uint32_t     HandshakeToken                   = 0;
    DeviceType   Type                             = DeviceType::Unknown;
    uint32_t     ResolutionWidth                  = 0;
    uint32_t     ResolutionHeight                 = 0;

    OmniInstance() {}

    OmniInstance(uint8_t DevMIndex) { DevMapIndex = DevMIndex; }

    void Clear()
    {
        memset(InstanceName, 0, sizeof(InstanceName));
        InstanceIP = 0;
        memset(IPv4_String, 0, sizeof(IPv4_String));
        LinkState        = NetLinkState::INACTIVE;
        HandshakeToken   = 0;
        ResolutionWidth  = 0;
        ResolutionHeight = 0;
    }

    void Edit(char* InstanceName_, char* IPv4_String_, uint32_t InstanceIP_, DeviceMap DeviceID)
    {
        InstanceIP = InstanceIP_;
        strncpy(IPv4_String, IPv4_String_, 16);
        strncpy(InstanceName, InstanceName_, (OmniDevNameLen + 1));
        DevMapIndex = static_cast<uint8_t>(DevMapIndex);
    }
};

// Instance group entries are for one click workspace setups to avoid having to adjust device
// topology and automate connections to devices saved.
struct InstanceGroupEntry
{
    char       InstanceName[OmniDevNameLen + 1] = {};
    uint32_t   InstanceIP                       = 0;
    char       IPv4_String[16]                  = {};
    DeviceMap  DevMapIndex                      = DeviceMap::END;
    DeviceType Type                             = DeviceType::Unknown;
};

#define OmniGroupNameLen 31
#define OmniGroupSubLen 47

// Instance Group entries container
struct OmniInstanceGroup
{
    char                              GroupName[OmniGroupNameLen + 1] = {};
    char                              Subtitle[OmniGroupSubLen + 1]   = {};
    uint64_t                          DateCreated                     = 0;
    uint8_t                           DeviceCount                     = 0;
    bool                              State                           = false;
    std::array<InstanceGroupEntry, 8> Instances                       = {};
};

// SubStreamEntry Structure to be used in OmniActiveInstance containers to manage SubStream States
// SubStreamID is a handle synchronized on both ends of a stream to be used in.. anything.
// Synchronization happens the moment an instance completes a handshake automatically by using the
// same method as HTTP/2 and QUIC.
// Initiator uses odd SubStreamIDs and the receiver uses even SubStreamIDs (Initiator if IP1 < IP2)
// State indicates.. well.. state, Features..being features, Route being the directionality.
struct SubStreamEntry
{
    SubStreamID        ID        = 0;
    OmniNetSubStream*  SubStream = nullptr;
    SubStreamState     State     = SubStreamState::Idle;
    FeatureTypes       Feature   = FeatureTypes::ScreenLink;
    FeatureActionRoute Route     = FeatureActionRoute::Outbound;
};

// Feature Tyoes to FeatureFlags bit masks for managing which feature is active to who on what route
inline FeatureFlags FeatureTypeToFlag(FeatureTypes Feature)
{
    return static_cast<FeatureFlags>(1 << static_cast<uint8_t>(Feature));
}

// The main container for Active Omni Instances extending from the base class.
// Includes the main NetSession, Active Feature States, SubStream and TCP Stream Registry.
struct OmniActiveInstance : OmniInstance
{
    std::unique_ptr<OmniNetSession<OmniMTU>> InstanceSession;
    uint16_t                                 port          = 62485;
    uint32_t                                 OutboundFlags = FeatureFlags::fInactive;
    uint32_t                                 InboundFlags  = FeatureFlags::fInactive;
    uint32_t                                 ActiveFlags   = FeatureFlags::fInactive;

    std::unordered_map<SubStreamID, SubStreamEntry> SubStreamRegistry;
    std::atomic<SubStreamID>                        NextSubStreamID{1};

    std::unordered_map<uint32_t, std::shared_ptr<OmniTCPStream>> TCPStreamRegistry;
    static inline std::atomic<uint32_t>                          NextTCPStreamID{1};

    OmniActiveInstance() = default;

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
        DevMapIndex      = Instance.DevMapIndex;
        ResolutionWidth  = Instance.ResolutionWidth;
        ResolutionHeight = Instance.ResolutionHeight;
    }

    OmniActiveInstance(OmniActiveInstance&& Other) noexcept
        : OmniInstance(Other), InstanceSession(std::move(Other.InstanceSession)), port(Other.port),
          OutboundFlags(Other.OutboundFlags), InboundFlags(Other.InboundFlags),
          ActiveFlags(Other.ActiveFlags), SubStreamRegistry(std::move(Other.SubStreamRegistry)),
          NextSubStreamID(Other.NextSubStreamID.load(std::memory_order_relaxed)),
          TCPStreamRegistry(std::move(Other.TCPStreamRegistry))
    {
    }

    OmniActiveInstance& operator=(OmniActiveInstance&& Other) noexcept
    {
        if (this != &Other) {
            OmniInstance::operator=(Other);
            InstanceSession   = std::move(Other.InstanceSession);
            port              = Other.port;
            OutboundFlags     = Other.OutboundFlags;
            InboundFlags      = Other.InboundFlags;
            ActiveFlags       = Other.ActiveFlags;
            SubStreamRegistry = std::move(Other.SubStreamRegistry);
            NextSubStreamID.store(
                Other.NextSubStreamID.load(std::memory_order_relaxed), std::memory_order_relaxed
            );
            TCPStreamRegistry = std::move(Other.TCPStreamRegistry);
        }
        return *this;
    }

    // Fetch add 2 combined with the HTTP/2 / QUIC stream incremental method of Even and Odd numbers
    // for the Initiator and the Receiver makes Peer 2 Peer ID collision impossible.
    inline SubStreamID AllocateNextSubStreamID()
    {
        return NextSubStreamID.fetch_add(2, std::memory_order_relaxed);
    }

    inline void RegisterSubStream(
        SubStreamID        ID,
        OmniNetSubStream*  SubStream,
        FeatureTypes       Feature = FeatureTypes::ScreenLink,
        FeatureActionRoute Route   = FeatureActionRoute::Outbound,
        SubStreamState     State   = SubStreamState::Pending
    )
    {
        SubStreamRegistry[ID] = SubStreamEntry{ID, SubStream, State, Feature, Route};
    }

    SubStreamEntry* FindSubStream(SubStreamID ID)
    {
        auto Iter = SubStreamRegistry.find(ID);
        return (Iter != SubStreamRegistry.end()) ? &Iter->second : nullptr;
    }

    inline void SetSubStreamFeature(
        SubStreamID ID, FeatureTypes Feature, FeatureActionRoute Route = FeatureActionRoute::Inbound
    )
    {
        auto* Entry = FindSubStream(ID);
        if (Entry) {
            Entry->Feature = Feature;
            Entry->Route   = Route;
        }
    }

    inline void SetSubStreamState(SubStreamID ID, SubStreamState State)
    {
        auto* Entry = FindSubStream(ID);
        if (Entry) {
            Entry->State = State;
        }
    }

    inline void UnregisterSubStream(SubStreamID ID) { SubStreamRegistry.erase(ID); }

    inline std::vector<SubStreamID> GetSubStreams(FeatureTypes Feature) const
    {
        std::vector<SubStreamID> Result;
        for (const auto& [Id, Entry] : SubStreamRegistry) {
            if (Entry.Feature == Feature) {
                Result.push_back(Id);
            }
        }
        return Result;
    }

    std::shared_ptr<OmniTCPStream> FindTCPStream(uint32_t StreamID)
    {
        auto iter = TCPStreamRegistry.find(StreamID);
        return (iter != TCPStreamRegistry.end()) ? iter->second : nullptr;
    }

    inline void RegisterTCPStream(uint32_t StreamID, std::shared_ptr<OmniTCPStream> Stream)
    {
        TCPStreamRegistry[StreamID] = std::move(Stream);
    }

    inline void CloseTCPStream(uint32_t StreamID) { TCPStreamRegistry.erase(StreamID); }

    inline void SetFeatureState(FeatureTypes Feature, FeatureActionRoute Route, bool State)
    {
        uint32_t  Flag = 1 << static_cast<uint32_t>(Feature);
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
        uint32_t Flag  = 1 << static_cast<uint32_t>(Feature);
        uint32_t Flags = (Route == FeatureActionRoute::Outbound) ? OutboundFlags : InboundFlags;
        return (Flags & Flag) != 0;
    }

    inline bool GetFeatureState(FeatureFlags Feature) const { return (ActiveFlags & Feature) != 0; }

    inline FeatureLinkState GetLinkState(FeatureTypes Feature) const
    {
        uint32_t Flag  = 1 << static_cast<uint32_t>(Feature);
        uint8_t  State = 0;
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
    char   Frame[MaxFrameLen];
    size_t FrameLen = 0;

    FrameByte(size_t frame_len) { FrameLen = frame_len; }
};

#endif // OMNIINSTANCES_H
