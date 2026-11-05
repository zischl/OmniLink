#pragma once

#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SessionHandler.hpp"
#include "WindowOperationTypes.hpp"

template <uint32_t MTU> class OmniNetSession;

namespace StreamControlPackets {

// Callback fired when a stream client finishes initializing its stream render window
// Sends a Stream Control Status Ready packet to the streamer.
// Handlers for this packet type are owned by capture streams for direct stream control.
inline void TransmitReady(OmniNetSession<OmniMTU>* Session, SubStreamID StreamKey)
{
    if (!Session) {
        return;
    }

    OmniStreamControlPacket Packet{};
    Packet.StreamKey = StreamKey;
    Packet.Command   = StreamControl::Ready;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcStreamControl;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniStreamControlPacket), Header);
}

// Requests an IDR keyframe from the remote sender
// Handlers for this packet type are owned by capture streams for direct stream control.
inline void TransmitRequestIDR(OmniNetSession<OmniMTU>* Session, SubStreamID StreamKey)
{
    if (!Session) {
        return;
    }

    OmniStreamControlPacket Packet{};
    Packet.StreamKey = StreamKey;
    Packet.Command   = StreamControl::RequestIDR;
    Packet.Param     = 0;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcStreamControl;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniStreamControlPacket), Header);
}

// Alerts the source capture stream to pause streaming frames for this SubStream
inline void TransmitPause(OmniNetSession<OmniMTU>* Session, SubStreamID StreamKey)
{
    if (!Session) {
        return;
    }

    OmniStreamControlPacket Packet{};
    Packet.StreamKey = StreamKey;
    Packet.Command   = StreamControl::Pause;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcStreamControl;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniStreamControlPacket), Header);
}

// Wakes up a paused stream on the host capture stream source
inline void TransmitResume(OmniNetSession<OmniMTU>* Session, SubStreamID StreamKey)
{
    if (!Session) {
        return;
    }

    OmniStreamControlPacket Packet{};
    Packet.StreamKey = StreamKey;
    Packet.Command   = StreamControl::Resume;

    OmniNet::OmniHeader Header{};
    Header.Target     = 0;
    Header.PacketType = OmniNet::PacketType::ProcStreamControl;
    Header.Flags      = 0;

    Session->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniStreamControlPacket), Header);
}

} // namespace StreamControlPackets
