#pragma once

#include "CaptureController.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "StreamWindow.hpp"
#include "SystemLinkContext.hpp"
#include "WinForge.hpp"
#include "WindowOperationTypes.hpp"

#include <Windows.h>
#include <climits>
#include <cstdint>
#include <d3d11.h>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct OmniSystemLink;
template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

struct SystemCaptureStreams
{
    // Feature Class Instance for CaptureStream controller
    OmniStreamer Streamer;

    // D3D Device and Context for Capture Streams
    ComPtr<ID3D11Device>        StreamingDevice  = nullptr;
    ComPtr<ID3D11DeviceContext> StreamingContext = nullptr;

    ~SystemCaptureStreams();

    // Accessors for Registry Containers
    StreamWindow*          FindStreamWindow(SubStreamID ID) const;
    OmniStreamer::StreamID FindStreamerID(SubStreamID ID) const;
    bool                   ContainsStreamWindow(SubStreamID ID) const;

    void RegisterStreamerID(SubStreamID ID, OmniStreamer::StreamID StreamID);
    void UnregisterStreamerID(SubStreamID ID);
    void RemoveCaptureStream(SubStreamID ID);

    // Base function to create a DXGI/WGC capture stream.
    OmniStreamer::StreamID AddCaptureStream(
        OmniGraphicsContext& GraphicsContext,
        OmniNetSubStream*    SubStream,
        DeviceMap            DeviceID,
        CaptureMode          Mode,
        const StreamConfig&  Config = {}
    );

    // Sends a Window Resize Event packet over the network.. the end..
    void TransmitWindowResize(
        OmniNetSession<OmniMTU>* Session,
        SubStreamID              WindowKey,
        DeviceMap                DeviceID,
        uint32_t                 Width,
        uint32_t                 Height
    );

    // Processes stream control packets received from the peer
    void HandleStreamControlPacket(
        SystemLinkContext&             Ctx,
        const OmniStreamControlPacket& Packet,
        DeviceMap                      SenderDevice = DeviceMap::C0
    );

    // Callback fired when WinForge finishes initializing its decoder
    void OnStreamWindowReady(
        OmniNetSession<OmniMTU>* Session, SubStreamID WindowKey, DeviceMap DeviceID
    );

    // Requests an IDR keyframe from the remote sender
    void
    RequestStreamIDR(OmniNetSession<OmniMTU>* Session, SubStreamID WindowKey, DeviceMap DeviceID);

    // Base function to create an async WinForge stream window.
    StreamWindow* CreateStreamWindow(
        const WindowCreationData& WindowData,
        int                       ShowCmd   = SW_SHOWNORMAL,
        HINSTANCE                 hInstance = nullptr
    );

    // Handles the OnClose event of a stream window, closes the sub stream, destroys the window.
    // FYI it's delegating work to a thread to let the window proc msg end to safely deconstruct.
    void OnStreamWindowClose(
        std::function<void(DeviceMap, SubStreamID, bool)> ReleaseSubStream,
        SubStreamID                                       WindowKey,
        DeviceMap                                         DeviceID
    );

    // Processes Window Resize Events for both ends, simply looks up the correct HWND and resizes
    void HandleStreamWindowResize(const OmniWinResizePacket Packet);

    // Processes Window Mouse Events, routing either to the specific target HWND (WindowLink)
    // or falling back to full-desktop KVM (ScreenLink)
    void HandleStreamWindowMouse(const OmniWinMousePacket& Packet);

    // Processes Window Key Events, routing either to the specific target HWND (WindowLink)
    // or falling back to full-desktop KVM (ScreenLink)
    void HandleStreamWindowKey(const OmniWinKeyPacket& Packet);

    // Creates a Stream Window to receive a capture stream over the network.
    // Additionally sets up window event handling such as OnInput, OnResize and OnWindowClose.
    // Returns the StreamWindow's populated frame buffer PoolConfig
    // Frame bytes are expected to be written to the pool after which gets decoded and rendered
    OmniNet::PoolConfig SetupStreamRenderWindow(
        SystemLinkContext&             Ctx,
        SubStreamID                    SubStreamID,
        DeviceMap                      DeviceID,
        uint32_t                       Width         = 0,
        uint32_t                       Height        = 0,
        int16_t                        X             = 0,
        int16_t                        Y             = 0,
        int                            ShowCmd       = SW_SHOW,
        std::string_view               Title         = "Stream Window",
        const OmniWindowEventHandlers* EventHandlers = nullptr
    );

    void DestroyStreamRenderWindow(SubStreamID SubStreamID);

    OmniNet::PoolConfig GetStreamWindowPoolConfig(SubStreamID SubStreamID);

  private:
    std::unordered_map<SubStreamID, StreamWindow*>          StreamWindowRegistry;
    std::unordered_map<SubStreamID, OmniStreamer::StreamID> StreamerIDRegistry;
};
