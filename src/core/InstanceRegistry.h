#ifndef OmniInstanceReg_H
#define OmniInstanceReg_H

#pragma once
#include "OmniConfig.h"
#include "SessionHandler.h"
#include <memory>
#include <type_traits>
#include <utility>

#include <bit>
#include <mutex>
#include <string>
#include <unordered_map>

#include "NetUtils.h"
#include "OmniDiscovery.h"
#include "OmniEnums.h"
#include "OmniInstances.h"
#include "OmniLogger.h"
#include "PlatformIntrinsics.h"
#include "system_probe_impl.h"

#include <cstdint>

struct OmniInstanceRegistry
{
  protected:
    std::mutex Mutex;
    OmniDiscovery* InstanceProbe = nullptr;
    uint32_t OpenSlotMask = 0x1FF;

  public:
    std::unordered_map<DeviceMap, OmniInstance> AllInstances = {};

    ActiveInstanceContainer ActiveInstances;

    // IP to DeviceMap Lookup
    std::unordered_map<uint32_t, DeviceMap> InstanceLookup = {};

    const OmniActiveInstance& UserInstance = ActiveInstances[DeviceMap::C0];

    OmniInstanceRegistry()
    {
        // Getting user data and initializing user instance
        uint32_t LocalIP;
        Device::RetrieveLocalIP(LocalIP, 0);
        if (LocalIP != 0) {
            IP2Char(LocalIP, AllInstances[DeviceMap::C0].IPv4_String);
        }

        AllInstances[DeviceMap::C0].InstanceIP = LocalIP;

        // Getting user data and initializing user instance
        Device::RetrieveUserName(AllInstances[DeviceMap::C0].InstanceName);

        // Creating an instance scanner object. Passing in local device name plus the
        // IP and then the port to use.
        InstanceProbe =
            new OmniDiscovery(AllInstances[DeviceMap::C0].InstanceName, LocalIP, OmniDiscoveryPort);
        InstanceProbe->Scan(15);

        ActivateInstance(DeviceMap::C0, nullptr);
    }

    ~OmniInstanceRegistry() { delete InstanceProbe; }

    std::unordered_map<DeviceMap, OmniInstance>* GetAvailableInstances() noexcept
    {
        return &AllInstances;
    }

    inline int GetAllInstancesCount() { return std::popcount(OpenSlotMask); }

    inline void AddInstance(uint32_t IP, DeviceMap DeviceID = DeviceMap::END)
    {
        DeviceMap OpenSlot;

        if (DeviceID == DeviceMap::END) {
            unsigned long BitIndex = BitScan(OpenSlotMask);
            OpenSlot = static_cast<DeviceMap>(BitIndex);
        } else {
            if ((OpenSlotMask & (1U << DeviceID))) {
                OpenSlot = DeviceID;
            };
        }

        AllInstances[OpenSlot].InstanceIP = IP;
        std::sprintf(
            AllInstances[OpenSlot].IPv4_String,
            "%u.%u.%u.%u",
            (IP >> 24) & 0xFF,
            (IP >> 16) & 0xFF,
            (IP >> 8) & 0xFF,
            IP & 0xFF
        );
        AllInstances[OpenSlot].DevMapIndex = OpenSlot;

        InstanceLookup[IP] = OpenSlot;

        OpenSlotMask &= ~(1 << static_cast<uint8_t>(OpenSlot));
    }

    inline void ClearInstance(DeviceMap slot)
    {
        AllInstances[slot].Clear();
        OpenSlotMask |= (1 << static_cast<uint8_t>(slot));
    }

    inline void
    ActivateInstance(DeviceMap DeviceID, std::unique_ptr<OmniNetSession<OmniMTU>> NetSession)
    {
        ActiveInstances[DeviceID] = OmniActiveInstance(AllInstances[DeviceID]);
        ActiveInstances[DeviceID].InstanceSession = std::move(NetSession);
    }

    // Check whether new scan results are available and get them if so
    // Otherwise initiate a new scana, uses bit masking to store open slots
    using DefaultCallbackType = decltype([]() {});
    template <typename DiscoveryCallback = DefaultCallbackType>
    void RefreshInstanceList(DiscoveryCallback&& Callback = {})
    {
        if (!InstanceProbe->ScanState.load()) {
            InstanceProbe->Scan(15);
            return;
        }

        std::unordered_map<uint32_t, std::string> AvailableInstances = InstanceProbe->get();

        std::lock_guard<std::mutex> lock(Mutex);

        for (auto& [DevMapIdx, Instance] : AllInstances) {
            if (Instance.InstanceIP != 0 && !AvailableInstances.contains(Instance.InstanceIP)) {
                ClearInstance(DevMapIdx);
            }
        }

        for (const auto& [IP, Name] : AvailableInstances) {
            if (InstanceLookup.contains(IP)) {
                OmniInstance& Instance = AllInstances[InstanceLookup[IP]];
                if (Instance.InstanceName[0] == '\0') {
                    snprintf(
                        Instance.InstanceName, sizeof(Instance.InstanceName), "%s", Name.c_str()
                    );
                }
                continue;
            }

            if (OpenSlotMask == 0) {
                Logger::log("Why u trynna link 9 computers at home..");
                break;
            }

            AddInstance(IP);
        }

        using DecayedType = std::decay_t<DiscoveryCallback>;

        if constexpr (!std::is_same_v<DecayedType, DefaultCallbackType>) {
            std::forward<DiscoveryCallback>(Callback)();
        }
    }

    template <typename DiscoveryCallback> void AwaitNewInstances(DiscoveryCallback&& Callback)
    {
        InstanceProbe->AwaitInstances(
            [this, Callback = std::forward<DiscoveryCallback>(Callback)](ProbeEvent Event) {
                RefreshInstanceList();
                Callback(Event);
            }
        );
    }

    void SwapInstances(DeviceMap DeviceID1, DeviceMap DeviceID2)
    {
        if (AllInstances[DeviceMap(DeviceID2)].InstanceIP == NULL) {
            AllInstances[DeviceMap(DeviceID2)].InstanceIP =
                AllInstances[DeviceMap(DeviceID1)].InstanceIP;
            AllInstances[DeviceMap(DeviceID2)].DevMapIndex = static_cast<uint8_t>(DeviceID1);
            strncpy(
                AllInstances[DeviceMap(DeviceID2)].InstanceName,
                AllInstances[DeviceMap(DeviceID1)].InstanceName,
                OmniDevNameLen
            );
            strncpy(
                AllInstances[DeviceMap(DeviceID2)].IPv4_String,
                AllInstances[DeviceMap(DeviceID1)].IPv4_String,
                16
            );

            AllInstances[DeviceMap(DeviceID1)].Clear();

        }

        else {
            OmniInstance temp = AllInstances[static_cast<DeviceMap>(DeviceID1)];
            AllInstances[DeviceMap(DeviceID1)].Edit(
                AllInstances[DeviceMap(DeviceID2)].InstanceName,
                AllInstances[DeviceMap(DeviceID2)].IPv4_String,
                AllInstances[DeviceMap(DeviceID2)].InstanceIP,
                DeviceMap(DeviceID2)
            );

            AllInstances[DeviceMap(DeviceID2)].Edit(
                temp.InstanceName, temp.IPv4_String, temp.InstanceIP, DeviceMap(temp.DevMapIndex)
            );
        }
    }

    // Handshake token... going places
    inline void TransmitConnectionRequest(DeviceMap DeviceID, uint32_t HandshakeToken)
    {
        OmniPayloadBase Payload{};
        Payload.Type = PayloadType::LinkRequest;
        Payload.PayloadLen = static_cast<uint16_t>(sizeof(uint32_t));

        std::memcpy(Payload.Payload, &HandshakeToken, sizeof(uint32_t));

        InstanceProbe->SendCustomPayload(
            AllInstances[DeviceID].InstanceIP, OmniDiscoveryPort, Payload
        );
    }

    // Yes.. all state transmissions have the HandshakeToken for validation
    inline void TransmitConnectionState(DeviceMap DeviceID)
    {
        uint32_t Token = AllInstances[DeviceID].HandshakeToken;
        OmniPayloadBase Payload{};
        Payload.Type = PayloadType::LinkResponse;
        Payload.PayloadLen = static_cast<uint16_t>(sizeof(uint8_t) + sizeof(uint32_t));
        Payload.Payload[0] = static_cast<char>(AllInstances[DeviceID].LinkState);

        std::memcpy(Payload.Payload + 1, &Token, sizeof(uint32_t));

        InstanceProbe->SendCustomPayload(
            AllInstances[DeviceID].InstanceIP, OmniDiscoveryPort, Payload
        );
    }

    inline bool SetConnectionState(DeviceMap DeviceID, NetLinkState NewLinkState)
    {
        NetLinkState CurrentState = AllInstances[DeviceID].LinkState;
        if (NewLinkState == NetLinkState::FAILED || NewLinkState == NetLinkState::INACTIVE) {
            AllInstances[DeviceID].LinkState = NewLinkState;
            return true;
        }
        if (NewLinkState > CurrentState) {
            AllInstances[DeviceID].LinkState = NewLinkState;
            return true;
        }
        Logger::log(
            "Blocked State Transition {} -> {} for device {}",
            static_cast<int>(CurrentState),
            static_cast<int>(NewLinkState),
            AllInstances[DeviceID].InstanceName
        );
        return false;
    }

    inline NetLinkState GetConnectionState(DeviceMap DeviceID)
    {
        return AllInstances[DeviceID].LinkState;
    }

    inline bool GetSessionState(DeviceMap DeviceID)
    {
        return ActiveInstances[DeviceID].InstanceSession != nullptr;
    }

    inline void SetHandshakeToken(DeviceMap DeviceID, uint32_t Token)
    {
        AllInstances[DeviceID].HandshakeToken = Token;
    }

    inline uint32_t GetHandshakeToken(DeviceMap DeviceID)
    {
        return AllInstances[DeviceID].HandshakeToken;
    }

    inline void ResetInstance(DeviceMap DeviceID)
    {
        if (ActiveInstances.count(DeviceID)) {
            ActiveInstances[DeviceID].InstanceSession.reset();
            ActiveInstances.erase(DeviceID);
        }
        AllInstances[DeviceID].LinkState = NetLinkState::INACTIVE;
        AllInstances[DeviceID].HandshakeToken = 0;
    }
};

#endif
