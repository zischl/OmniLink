#pragma once

#include "OmniEnums.h"
#include <cstdint>

#pragma pack(push, 1)

enum class WinDragAction : uint8_t { Begin = 0, Move = 1, Drop = 2, Cancel = 3 };

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
    uint16_t      WindowID;     // SubStream identifier for the dragged window
};

#pragma pack(pop)

static_assert(sizeof(OmniWinDragPacket) == 16, "OmniWinDragPacket must be exactly 16 bytes");
