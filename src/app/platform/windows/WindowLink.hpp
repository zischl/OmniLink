#pragma once

#include "CaptureController.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SystemLinkContext.hpp"
#include "WindowDragCap.hpp"
#include "WindowOperationTypes.hpp"

#include <Windows.h>
#include <climits>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

template <uint32_t MTU> class OmniNetSession;

enum class DragPhase : uint8_t { Idle, LocalHover, Crossing, Remote, Returning };

// Exactly what it means, holds drag session data.
struct WindowDragSession
{
    DragPhase   Phase      = DragPhase::Idle;
    SubStreamID StreamID   = 0;
    DeviceMap   TargetEdge = DeviceMap::C0;
    HWND        SourceHwnd = NULL;
    int         GripX      = 0;
    int         GripY      = 0;
    uint32_t    ResW       = 0;
    uint32_t    ResH       = 0;
    RECT        LastPos    = {};
};

// WindowLink client session for storing session data while it lasts.
// Only stored on the client side, the receiving end of the stream.
// WindowHwnd is the stream window's handle.
struct WindowStreamEntry
{
    OmniNetSession<OmniMTU>*                          Session       = nullptr;
    OmniSpatialRouter*                                SpatialRouter = nullptr;
    OmniRouter*                                       Router        = nullptr;
    std::function<void(DeviceMap, SubStreamID, bool)> ReleaseSubStream;
    DeviceMap                                         DeviceID        = DeviceMap::C0;
    SubStreamID                                       WindowKey       = 0;
    HWND                                              WindowHwnd      = NULL;
    int                                               LastX           = INT_MIN;
    int                                               LastY           = INT_MIN;
    int                                               LastSentX       = INT_MIN;
    int                                               LastSentY       = INT_MIN;
    DWORD                                             LastMoveMsgTime = 0;
    EdgeSpatiality                                    SpatialState    = EdgeSpatiality::Local;
};

struct SystemWindowLink
{
    // Feature Class Instance for Drag Detection
    OmniDragLink DragLink;

    // Map/Reverse Map Hwnd with Sub Streams for the DragDetection in WindowLink
    // FYI the first 2 are for the source window handles
    std::unordered_map<HWND, SubStreamID>                               Hwnd2SubStreamRegistry;
    std::unordered_map<SubStreamID, HWND>                               SubStream2HwndRegistry;
    std::unordered_map<SubStreamID, DeviceMap>                          SubStream2DeviceRegistry;
    std::unordered_map<SubStreamID, std::unique_ptr<WindowStreamEntry>> WindowStreamEntries;

    WindowDragSession DragSession;

    // Window Focus helper
    static void EnsureWindowFocus(HWND TargetHwnd);

    // State transitions
    void OnLocalDragStart(HWND Hwnd, const RECT& WindowPos, POINT CursorPos);
    bool OnEdgeCrossBegin(
        SystemLinkContext& Ctx, HWND Hwnd, DeviceMap TargetEdge, const RECT& WindowPos
    );
    void OnCrossCancel(SystemLinkContext& Ctx, HWND Hwnd);
    void OnRemoteDrop(SystemLinkContext& Ctx, HWND Hwnd);
    void OnReturnBegin(DeviceMap DeviceID, HWND SourceHwnd, int TargetX, int TargetY);
    void OnReturnDrop(DeviceMap DeviceID, HWND SourceHwnd);

    // Local events & Forwarders
    bool TryInitiateDragFromCaptionTest(
        SystemLinkContext& Ctx,
        SubStreamID        StreamID,
        HWND               TargetHwnd,
        POINT              CursorPoint,
        RECT               WinRect
    );
    OmniNet::PoolConfig SetupClientStreamRenderWindow(
        SystemLinkContext& Ctx,
        SubStreamID        SubStreamID,
        DeviceMap          DeviceID,
        uint32_t           Width,
        uint32_t           Height,
        int16_t            TargetX,
        int16_t            TargetY
    );

    // Called by DragLink.OnDragMove on each tick
    // Resolves the active edge, maintains the Begin/Move/Cancel state machine
    // Also.. sends the appropriate WinDrag network packets.
    void
    HandleWindowDragMove(SystemLinkContext& Ctx, HWND Hwnd, const RECT& WindowPos, POINT CursorPos);

    // Called by DragLink.OnDragEnd on mouse release
    // sends the final Drop packet if the drag was in transit to a remote edge
    // if this window was returning from a remote edge, recall it in-place
    // Also.. yes.. resets drag state for the next drag
    void HandleWindowDragEnd(SystemLinkContext& Ctx, HWND Hwnd);

    // Handles the window drag grab when cursor returns from client to host with left mouse down
    void HandleReturningWindow(DeviceMap DeviceID);

    // For the given SubStream, DeviceID and window HWND handle, boot up and setup a capture stream
    // Populates the StreamLookup Registries
    OmniStreamer::StreamID StartWindowCaptureStream(
        SystemLinkContext& Ctx, SubStreamID SubStreamID, DeviceMap DeviceID, HWND Hwnd
    );

    // Removes window capture stream, cleans up stream lookup mappings
    void StopWindowCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID);

    // Callback for WindowLink stream source window
    // Called upon a DragLink trigger when an edge crossing drag event begins, Drops, Cancels
    // Although move is handled here it's not called, unused for now
    SubStreamID HandleWindowDragEvent(
        SystemLinkContext& Ctx, HWND Hwnd, DeviceMap TargetDevice, WinDragAction Action
    );

    // Inbound network handlers

    // Handles drag events on the Host for the captured application window of a given DeviceID.
    // Excpects normalized ratios to 0-65535 range for target X and Y coords.
    // why ? it's more efficient that way and not dependant on fragile resolution shit.
    void HandleStreamSourceDrag(
        SystemLinkContext& Ctx, const OmniWinDragPacket& Packet, DeviceMap DeviceID, HWND Hwnd
    );

    // Handles drag events on the Client for the stream render window of a given DeviceID
    // Again... Excpects normalized ratios to 0-65535 range for target X and Y coords
    // why ? it's more efficient that way and not dependant on fragile resolution shit.
    void HandleStreamClientDrag(
        SystemLinkContext& Ctx, const OmniWinDragPacket& Packet, DeviceMap DeviceID
    );

    // Handles incoming window resize event for source windows on the host
    // Meaning... the stream window got resized
    bool HandleStreamWindowResize(const OmniWinResizePacket& Packet);

    // Handles incoming window mouse event for source windows on the host
    // Meaning... the stream window got some mouse input
    bool HandleStreamWindowMouse(SystemLinkContext& Ctx, const OmniWinMousePacket& Packet);

    // Handles incoming window key event for source windows on the host
    // Meaning... the stream window got some keyboard input
    void HandleStreamWindowKey(const OmniWinKeyPacket& Packet);

    // Desktop positioning & Window recovery

    // Centers a window on the desktop given the target HWND, self explanatory..
    void CenterWindowOnDesktop(HWND TargetHwnd);

    // Recalls a specific streamed window back to the host desktop
    // Notifying Client to destroy its stream render window and..
    // Stops capture stream on Host, releases SubStream on Host
    // Finally.. Restores source window on Host desktop and centers if CenterOnDesktop is true
    bool RecallWindow(SystemLinkContext& Ctx, HWND SourceHwnd, bool CenterOnDesktop = true);

    // Recalls all active streamed windows back to the host desktop
    // Just.. a loop on RecallWindow
    size_t RecallAllStreamingWindows(SystemLinkContext& Ctx);

    OmniNet::PoolConfig SetState(
        SystemLinkContext& Ctx,
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );
};
