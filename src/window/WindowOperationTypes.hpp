#pragma once

#include "OmniEnums.hpp"
#include <cstdint>

#pragma pack(push, 1)

// Packet command states for a window drag action. quite obvious isn't it..
enum class WinDragAction : uint8_t { Begin = 0, Move = 1, Drop = 2, Cancel = 3 };

// States for Stream Context positional/boundary state for a window.
// Local means within local monitor bounds.
// InTransit represents between 2 spaces in a physical monitor boundary seam.
// Position mirroring is active in InTransit state.
// Remote represents fully positioned on remote peer monitor either streaming or parked on host
enum class EdgeSpatiality : uint8_t { Local = 0, InTransit = 1, Remote = 2 };

struct alignas(16) OmniWinDragPacket
{
    WinDragAction Action;       // See the enum up there ? yes.. that..
    DeviceMap     Edge;         // Screen edge
    int16_t       WindowX;      // Target window X coordinate (This can go negative fyi)
    int16_t       WindowY;      // Target window Y coordinate (Same Here ^^^)
    uint16_t      WindowWidth;  // Window width
    uint16_t      WindowHeight; // Window height
    int16_t       CursorGripX;  // Cursor X offset relative to window top-left
    int16_t       CursorGripY;  // Same but for Y
    SubStreamID   WindowID;     // SubStream identifier for the dragged window
};

#pragma pack(pop)

static_assert(sizeof(OmniWinDragPacket) == 16, "OmniWinDragPacket must be exactly 16 bytes");

#pragma pack(push, 1)

struct alignas(8) OmniWinResizePacket
{
    SubStreamID WindowKey;
    uint16_t    Width;
    uint16_t    Height;
    uint16_t    Reserved;
};

#pragma pack(pop)

static_assert(sizeof(OmniWinResizePacket) == 8, "shit must be exactly 8 bytes");

#pragma pack(push, 1)

struct alignas(16) OmniWinMousePacket
{
    uint64_t SubStreamID; // Stream window identifier
    uint16_t NormalizedX; // Normalized client X, do I even need to comment..
    uint16_t NormalizedY; // ^^^ but for Y
    uint16_t Buttons;     // Button flags
    int16_t  Wheel;       // Scroll delta
};

#pragma pack(pop)

static_assert(sizeof(OmniWinMousePacket) == 16, "OmniWinMousePacket must be exactly 16 bytes");

#pragma pack(push, 1)

struct alignas(16) OmniWinKeyPacket
{
    uint64_t SubStreamID; // Stream window identifier
    uint16_t VkCode;      // Virtual Key Code
    uint16_t ScanCode;    // Hardware Scan Code
    uint16_t Flags;       // Key press event flags
    uint16_t Reserved;    // Just padding
};

#pragma pack(pop)

static_assert(sizeof(OmniWinKeyPacket) == 16, "OmniWinKeyPacket must be exactly 16 bytes");

enum class StreamControl : uint8_t {
    Ready      = 0, // Decoder & swapchain ready to receive
    RequestIDR = 1, // Request an IDR keyframe
    Pause      = 2, // Stream flow paused
    Resume     = 3, // Stream flow resumed
};

#pragma pack(push, 1)

struct alignas(8) OmniStreamControlPacket
{
    SubStreamID   StreamKey; // Target SubStream ID
    StreamControl Command;   // Control action
    uint8_t       Reserved;  // Padding
    uint32_t      Param;     // Parameter / reason
};

#pragma pack(pop)

static_assert(
    sizeof(OmniStreamControlPacket) == 8, "OmniStreamControlPacket must be exactly 8 bytes"
);
