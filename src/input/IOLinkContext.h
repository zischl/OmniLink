#pragma once

#include "OmniConfig.h"
#include "OmniEnums.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

template <uint32_t MTU> class OmniNetSession;

// Shared state between OmniIOCap and OmniIOShield.
struct IOLinkContext
{
    std::atomic<OmniNetSession<OmniMTU>*> ActiveNetSession{nullptr};
    DeviceMap                             ActiveEdge{DeviceMap::C0};

    std::atomic<bool> InputLocked{false};

    uint32_t ResWidth{0};
    uint32_t ResHeight{0};

    std::unordered_map<DeviceMap, OmniNetSession<OmniMTU>*> SessionTable;
    std::mutex                                              SessionMutex;

    void RegisterSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session)
    {
        std::lock_guard Lock(SessionMutex);
        SessionTable[DeviceID] = Session;
    }

    void UnregisterSession(DeviceMap DeviceID)
    {
        std::lock_guard Lock(SessionMutex);
        SessionTable.erase(DeviceID);
        if (ActiveEdge == DeviceID) {
            ActiveNetSession.store(nullptr, std::memory_order_release);
            InputLocked.store(false, std::memory_order_release);
            ActiveEdge = DeviceMap::C0;
        }
    }

    void ActivateEdge(DeviceMap DeviceID)
    {
        std::lock_guard Lock(SessionMutex);
        auto            It = SessionTable.find(DeviceID);
        ActiveNetSession.store(
            It != SessionTable.end() ? It->second : nullptr, std::memory_order_release
        );
        ActiveEdge = DeviceID;
    }

    void DeactivateEdge()
    {
        std::lock_guard Lock(SessionMutex);
        ActiveNetSession.store(nullptr, std::memory_order_release);
        InputLocked.store(false, std::memory_order_release);
        ActiveEdge = DeviceMap::C0;
    }

    void Reset()
    {
        std::lock_guard Lock(SessionMutex);
        SessionTable.clear();
        ActiveNetSession.store(nullptr, std::memory_order_release);
        InputLocked.store(false, std::memory_order_release);
        ActiveEdge = DeviceMap::C0;
    }
};
