#pragma once

#include "OmniConfig.hpp"
#include "OmniEnums.hpp"
#include "OmniRouterContext.hpp"

#include <atomic>
#include <cstdint>

#if defined(_WIN32)
#include <basetsd.h>
constexpr ULONG_PTR OMNI_INPUT_COOKIE = 0x4F4D4E49;
#else
constexpr uintptr_t OMNI_INPUT_COOKIE = 0x4F4D4E49;
#endif

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

// OmniEdgeCrossPacket is to enter another instance and transfer ownership of the cursor.
// EdgeRelayMask is to let the new cursor owner know what other instances are in which direction.
// This lets the InputLink work as a mesh even if not every device is interconnected.
// X, Y.. think of it as a percentage of monitor position compared to monitor resolution. bye !
struct alignas(16) OmniEdgeCrossPacket
{
    DeviceMap Edge;          // DeviceMap edge
    uint8_t   EdgeRelayMask; // IO Active Device Grid based nearby Instances
    uint16_t  Y_Ratio;       // Normalized Y ratio (0..65535)
    uint16_t  X_Ratio;       // Normalized X ratio (0..65535)
    uint16_t  Reserved;      // Padding
    uint64_t  Reserved2;     // Padding to 16 bytes
};

struct alignas(16) OmniEdgeRelayPacket
{
    DeviceMap RelayEdge; // DeviceMap edge
    uint8_t   Reserved;  // Padding..
    uint16_t  Y_Ratio;   // Normalized Y ratio (0..65535)
    uint16_t  X_Ratio;   // Normalized X ratio (0..65535)
    uint16_t  Reserved2; // Padding
    uint64_t  Reserved3; // Padding to 16 bytes
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

static_assert(sizeof(OmniMousePacket) == 16, "SIMD optimizations...");
static_assert(sizeof(OmniEdgeCrossPacket) == 16, "SIMD optimizations...");
static_assert(sizeof(OmniKeyPacket) == 16, "SIMD optimizations...");

// Shared input state between OmniInputLink and OmniInputFilter.
struct InputLinkContext
{
    OmniRouter& Router;

    std::atomic<DeviceMap> ActiveRemoteEdge{DeviceMap::C0};

    std::atomic<bool> InputLocked{false};

    explicit InputLinkContext(OmniRouter& Router_) : Router(Router_) {}

    void ActivateRemoteEdge(DeviceMap DeviceID)
    {
        InputLocked.store(true, std::memory_order_release);
        ActiveRemoteEdge.store(DeviceID, std::memory_order_release);
    }

    void DeactivateRemoteEdge()
    {
        InputLocked.store(false, std::memory_order_release);
        ActiveRemoteEdge.store(DeviceMap::C0, std::memory_order_release);
    }

    void ResetEdge()
    {
        InputLocked.store(false, std::memory_order_release);
        ActiveRemoteEdge = DeviceMap::C0;
    }
};
