#include "StreamSessions.hpp"
#include "IOLinkContext.hpp"
#include "OmniRouterContext.hpp"
#include "SpatialRouter.hpp"
#include "StreamControlPackets.hpp"
#include "StreamPacketDispatch.hpp"
#include "WindowLink.hpp"

#include <dwmapi.h>

// Handlers for client stream render window events...

// Dispatches normalized mouse events over the network to the Host source window
static void StreamWindowOnMouseInput(void* Ctx, const OmniMousePacket& Packet)
{
    auto* StreamContext = static_cast<WindowStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->Session)
        return;

    StreamPackets::TransmitMousePacket(
        StreamContext->Session,
        StreamContext->WindowKey,
        static_cast<uint16_t>(Packet.dX),
        static_cast<uint16_t>(Packet.dY),
        Packet.Buttons,
        Packet.Wheel
    );
}

// Dispatches virtual key input over the network to the Host source window
static void StreamWindowOnKeyInput(void* Ctx, const OmniKeyPacket& Packet)
{
    auto* StreamContext = static_cast<WindowStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->Session)
        return;

    StreamPackets::TransmitKeyPacket(
        StreamContext->Session,
        StreamContext->WindowKey,
        Packet.VkCode,
        Packet.ScanCode,
        Packet.Flags
    );
}

// Fired when the client resizes the stream render window
// Syncs new dimensions back to the source window
// Also asks for an IDR frame so that it doesn't look like garbage
static void StreamWindowOnResize(void* Ctx, uint32_t NewWidth, uint32_t NewHeight)
{
    auto* StreamContext = static_cast<WindowStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->Session)
        return;

    if (StreamContext->SpatialState == EdgeSpatiality::Remote)
        return;

    StreamPackets::TransmitResizePacket(
        StreamContext->Session,
        StreamContext->WindowKey,
        static_cast<uint16_t>(NewWidth),
        static_cast<uint16_t>(NewHeight)
    );
    StreamControlPackets::TransmitRequestIDR(StreamContext->Session, StreamContext->WindowKey);
}

// Callback fired when WinForge decoder initializes on client
// Kicks the host streamer into active gear as the stream is stalled on Ready state
static void StreamWindowOnDecoderReady(void* Ctx)
{
    auto* StreamContext = static_cast<WindowStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->Session)
        return;

    StreamControlPackets::TransmitReady(StreamContext->Session, StreamContext->WindowKey);
}

// Handles the OnClose event of a stream window, closes the sub stream, releases on peer
// FYI it's delegating work to let the window proc msg end to safely deconstruct.
// note to me in the future, need to send an alert back to the host..
static void StreamWindowOnWindowClose(void* Ctx)
{
    auto* StreamContext = static_cast<WindowStreamEntry*>(Ctx);
    if (StreamContext && StreamContext->ReleaseSubStream) {
        StreamContext->ReleaseSubStream(StreamContext->DeviceID, StreamContext->WindowKey, true);
    }
}

// Called on each drag move tick on the client stream window
// Computes reverse edge crossings back to Host and manages transit states
// Also.. includes message time rate-limiting to avoid flooding the socket with drag packets
static void StreamWindowOnWindowDragMove(void* Ctx, int CurrentX, int CurrentY)
{
    auto* StreamContext = static_cast<WindowStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->Session || !StreamContext->SpatialRouter)
        return;

    if (CurrentX == StreamContext->LastSentX && CurrentY == StreamContext->LastSentY)
        return;

    DWORD CurrentTime = GetMessageTime();
    if (CurrentTime - StreamContext->LastMoveMsgTime < 10)
        return;

    StreamContext->LastMoveMsgTime = CurrentTime;
    StreamContext->LastSentX       = CurrentX;
    StreamContext->LastSentY       = CurrentY;

    DeviceMap ReverseEdge = ComputeRelativeSpatialID(StreamContext->DeviceID, DeviceMap::C0);

    uint32_t ClientResW =
        StreamContext->Router ? StreamContext->Router->ResWidth.load(std::memory_order_relaxed) : 0;
    uint32_t ClientResH = StreamContext->Router
                              ? StreamContext->Router->ResHeight.load(std::memory_order_relaxed)
                              : 0;
    if (ClientResW == 0)
        ClientResW = GetSystemMetrics(SM_CXSCREEN);
    if (ClientResH == 0)
        ClientResH = GetSystemMetrics(SM_CYSCREEN);

    RECT ClientWindowRect{CurrentX, CurrentY, CurrentX + 1, CurrentY + 1};
    bool OverlapState =
        (StreamContext->SpatialRouter->EvaluateWindow(ClientWindowRect) == ReverseEdge);

    if (OverlapState) {
        StreamContext->SpatialState = EdgeSpatiality::InTransit;

        int16_t PacketX = 0, PacketY = 0;
        OmniSpatialRouter::EncodeEdgePosition(
            ReverseEdge, ClientWindowRect, ClientResW, ClientResH, PacketX, PacketY
        );

        StreamPackets::TransmitDragPacket(
            StreamContext->Session,
            WinDragAction::Move,
            StreamContext->WindowKey,
            ReverseEdge,
            PacketX,
            PacketY,
            0,
            0
        );

        POINT CursorPoint{};
        GetCursorPos(&CursorPoint);
        bool CursorEdge =
            (StreamContext->SpatialRouter->Evaluate(CursorPoint.x, CursorPoint.y) == ReverseEdge);

        if (CursorEdge) {
            StreamContext->SpatialState = EdgeSpatiality::Remote;
            ReleaseCapture();
            HWND TargetRenderHwnd = StreamContext->WindowHwnd;
            if (TargetRenderHwnd && IsWindow(TargetRenderHwnd)) {
                ShowWindow(TargetRenderHwnd, SW_HIDE);
            }
        }

    } else if (StreamContext->SpatialState == EdgeSpatiality::InTransit) {
        // Window just cleared the edge onto the Client screen
        // Sending one final Drop so Host parks the source window in the center
        StreamContext->SpatialState = EdgeSpatiality::Local;

        StreamPackets::TransmitDragPacket(
            StreamContext->Session,
            WinDragAction::Drop,
            StreamContext->WindowKey,
            ReverseEdge,
            static_cast<int16_t>(CurrentX),
            static_cast<int16_t>(CurrentY),
            0,
            0
        );
    }
}

// Called on mouse release when dragging a stream window on the client
// Commits the drop on the Host side if the window was moved
static void StreamWindowOnWindowDragRelease(void* Ctx, int FinalX, int FinalY)
{
    auto* StreamContext = static_cast<WindowStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->Session)
        return;

    if (StreamContext->SpatialState == EdgeSpatiality::Remote)
        return;

    StreamContext->SpatialState = EdgeSpatiality::Local;

    if (FinalX == StreamContext->LastX && FinalY == StreamContext->LastY)
        return;

    StreamContext->LastX = FinalX;
    StreamContext->LastY = FinalY;

    StreamPackets::TransmitDragPacket(
        StreamContext->Session,
        WinDragAction::Drop,
        StreamContext->WindowKey,
        ComputeRelativeSpatialID(StreamContext->DeviceID, DeviceMap::C0),
        static_cast<int16_t>(FinalX),
        static_cast<int16_t>(FinalY),
        0,
        0
    );
}
