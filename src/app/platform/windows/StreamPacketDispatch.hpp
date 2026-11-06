#pragma once

#include "StreamSessions.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SessionHandler.hpp"
#include "WindowOperationTypes.hpp"

template <uint32_t MTU> class OmniNetSession;

namespace StreamPackets {

// Transmits window drag packets (Begin, Move, Drop, Cancel) over the network
inline void TransmitDragPacket(
    OmniNetSession<OmniMTU>* Session,
    WinDragAction            Action,
    SubStreamID              WindowID,
    DeviceMap                Edge,
    int16_t                  WindowX,
    int16_t                  WindowY,
    uint16_t                 WindowWidth,
    uint16_t                 WindowHeight,
    int16_t                  GripX = 0,
    int16_t                  GripY = 0
)
{
    if (!Session) {
        return;
    }

    OmniWinDragPacket Packet{};
    Packet.Action       = Action;
    Packet.WindowID     = WindowID;
    Packet.Edge         = Edge;
    Packet.WindowX      = WindowX;
    Packet.WindowY      = WindowY;
    Packet.WindowWidth  = WindowWidth;
    Packet.WindowHeight = WindowHeight;
    Packet.CursorGripX  = GripX;
    Packet.CursorGripY  = GripY;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcWinDrag;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header);
}

// Sends normalized mouse coordinates and buttons over to the Host window
inline void TransmitMousePacket(
    OmniNetSession<OmniMTU>* Session,
    SubStreamID              SubStreamID,
    uint16_t                 NormX,
    uint16_t                 NormY,
    uint16_t                 Buttons,
    int16_t                  Wheel
)
{
    if (!Session) {
        return;
    }

    OmniWinMousePacket Packet{};
    Packet.SubStreamID = static_cast<uint64_t>(SubStreamID);
    Packet.NormalizedX = NormX;
    Packet.NormalizedY = NormY;
    Packet.Buttons     = Buttons;
    Packet.Wheel       = Wheel;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcWinMouse;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinMousePacket), Header);
}

// Sends virtual key input events to the target Host window
inline void TransmitKeyPacket(
    OmniNetSession<OmniMTU>* Session,
    SubStreamID              SubStreamID,
    uint16_t                 VkCode,
    uint16_t                 ScanCode,
    uint16_t                 Flags
)
{
    if (!Session) {
        return;
    }

    OmniWinKeyPacket Packet{};
    Packet.SubStreamID = static_cast<uint64_t>(SubStreamID);
    Packet.VkCode      = VkCode;
    Packet.ScanCode    = ScanCode;
    Packet.Flags       = Flags;
    Packet.Reserved    = 0;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcWinKey;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinKeyPacket), Header);
}

// Sends a Window Resize Event packet over the network.. do I eve need to comment ?
inline void TransmitResizePacket(
    OmniNetSession<OmniMTU>* Session, SubStreamID WindowKey, uint16_t Width, uint16_t Height
)
{
    if (!Session) {
        return;
    }

    OmniWinResizePacket Packet{};
    Packet.WindowKey = WindowKey;
    Packet.Width     = Width;
    Packet.Height    = Height;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcWinResize;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinResizePacket), Header);
}

} // namespace StreamPackets
