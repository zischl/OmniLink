#pragma once

#include "OmniConfig.h"
#include "OmniEnums.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#pragma pack(push, 1)

enum OmniMouseFlags : uint16_t {
    OMNI_MOUSE_RELATIVE = 0x0000,
    OMNI_MOUSE_ABSOLUTE = 0x0001,
    OMNI_MOUSE_WARP     = 0x0002,
};

struct alignas(16) OmniMousePacket
{
    int32_t  dX;       // X displacement or absolute target X
    int32_t  dY;       // Y displacement or absolute target Y
    uint16_t Buttons;  // MOUSEEVENTF_* button flags
    int16_t  Wheel;    // Wheel scroll delta
    uint16_t Flags;    // OmniMouseFlags
    uint16_t Reserved; // Padding to 16 bytes
};

enum class BoundaryAction : uint8_t { Enter = 0, Return = 1 };

struct alignas(16) OmniBoundaryPacket
{
    uint8_t  Action;    // BoundaryAction (0=Enter, 1=Return)
    uint8_t  Edge;      // DeviceMap edge
    uint16_t Y_Ratio;   // Normalized Y ratio (0..65535)
    uint16_t X_Ratio;   // Normalized X ratio (0..65535)
    uint16_t Reserved;  // Padding
    uint64_t Reserved2; // Padding to 16 bytes
};

struct alignas(16) OmniKeyPacket
{
    uint16_t VkCode;    // Virtual Key Code
    uint16_t ScanCode;  // Hardware Scan Code
    uint16_t Flags;     // KEYEVENTF_* flags
    uint16_t Reserved;  // Padding
    uint64_t ExtraInfo; // Additional info, or padding if not
};

#pragma pack(pop)

static_assert(sizeof(OmniMousePacket) == 16, "OmniMousePacket must be exactly 16 bytes");
static_assert(sizeof(OmniBoundaryPacket) == 16, "OmniBoundaryPacket must be exactly 16 bytes");
static_assert(sizeof(OmniKeyPacket) == 16, "OmniKeyPacket must be exactly 16 bytes");

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
