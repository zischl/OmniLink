#pragma once

#include "AudioCap.hpp"
#include "AudioRender.hpp"
#include "CaptureController.hpp"
#include "ClipBoardLink.h"
#include "ClipboardTypes.hpp"
#include "IOLink.hpp"
#include "IOLinkContext.hpp"
#include "OmniEnums.hpp"
#include "OmniGraphicsContext.hpp"
#include "OmniInstances.h"
#include "OmniPackets.hpp"
#include "OmniRouterContext.hpp"
#include "OmniTCPStream.h"
#include "StreamWindow.hpp"
#include "WindowDragCap.hpp"
#include "WindowOperationTypes.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Windows.h>
#include <shellapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

using NetworkPacketHandlerFn = void(char*, uint32_t, uint8_t, void*);
using SubStreamID            = uint16_t;

NetworkPacketHandlerFn NetworkPacketHandler;

struct OmniSystemLink
{
    // Main Window Data
    HINSTANCE hInstance = nullptr;
    int       nCmdShow  = 0;
    HWND      WindowID  = nullptr;

    // Active Instances borrowed from InstanceRegistry
    ActiveInstanceContainer* ActiveInstances = nullptr;

    // Feature Class Instances for main 5 Feature
    OmniRouter        OmniRouter;
    OmniStreamer      Streamer;
    OmniDragLink      DragLink{OmniRouter};
    InputLinkContext  InputLinkCtx{OmniRouter};
    OmniInputLink     InputLink{InputLinkCtx};
    OmniInputFilter   InputFilter{InputLinkCtx};
    OmniClipboardLink ClipboardLink;
    OmniAudioLink     AudioLink;

    // D3D Device and Context for Capture Streams
    OmniGraphicsContext&        GraphicsContext;
    ComPtr<ID3D11Device>        StreamingDevice  = nullptr;
    ComPtr<ID3D11DeviceContext> StreamingContext = nullptr;

    // StreamWindow Container and SubStream to StreamWindow / StreamWindowID lookup
    std::unordered_map<SubStreamID, StreamWindow*>          StreamWindowRegistry;
    std::unordered_map<SubStreamID, OmniStreamer::StreamID> StreamerIDRegistry;

    // Map/Reverse Map Hwnd with Sub Streams for the DragDetection in WindowLink
    // FYI these 2 are for the source window handles
    std::unordered_map<HWND, SubStreamID> Hwnd2SubStreamRegistry;
    std::unordered_map<SubStreamID, HWND> SubStream2HwndRegistry;

    struct WindowStreamContext
    {
        OmniSystemLink*          SysLink   = nullptr;
        OmniNetSession<OmniMTU>* Session   = nullptr;
        DeviceMap                DeviceID  = DeviceMap::C0;
        SubStreamID              WindowKey = 0;
    };

    std::unordered_map<SubStreamID, WindowStreamContext> StreamContexts;

    // SubStream request, release, and configuration callbacks.
    std::function<SubStreamID(DeviceMap, FeatureTypes)>                     RequestSubStream;
    std::function<void(DeviceMap, SubStreamID, bool)>                       ReleaseSubStream;
    std::function<void(DeviceMap, SubStreamID, const OmniNet::PoolConfig&)> ConfigureSubStream;

    // AudioStreams and renderers
    std::array<std::atomic<OmniNetSubStream*>, DeviceMap::END> ActiveAudioStreams{};
    std::atomic<uint32_t>                                      AudioStreamCount{0};
    std::map<SubStreamID, std::unique_ptr<AudioRender>>        AudioRenderers;

    // ClipboardLink Subscribers listed in atomic bitmask, TCP streams, event callbacks
    std::atomic<uint16_t>                                        ActiveClipboardSubscriptions{0};
    std::mutex                                                   ClipboardStreamsMutex;
    std::unordered_map<uint32_t, std::shared_ptr<OmniTCPStream>> ActiveClipboardStreams;
    std::function<void(const ClipboardStreamEvent&)>             OnClipboardStreamEvent;

    OmniSystemLink(OmniGraphicsContext& GraphicsContext);
    ~OmniSystemLink();

    void SetupSystemLink(HINSTANCE hInstance, int nCmdShow, HWND WindowID);

    // Base function to create a DXGI/WGC capture stream.
    OmniStreamer::StreamID AddCaptureStream(
        OmniNetSubStream*   SubStream,
        DeviceMap           DeviceID,
        CaptureMode         Mode,
        const StreamConfig& Config = {}
    );

    // Sends a Window Resize Event packet over the network.. the end..
    void TransmitWindowResizeEvent(
        SubStreamID WindowKey, DeviceMap DeviceID, uint32_t NewWidth, uint32_t NewHeight
    );

    // For the given SubStream and DeviceID, boot up and setup a DXGI screen capture stream
    // Populates the StreamLookup Registries
    OmniStreamer::StreamID StartScreenCaptureStream(SubStreamID SubStreamID, DeviceMap DeviceID);

    // Removes screen capture stream, cleans up stream lookup mappings
    void StopScreenCaptureStream(SubStreamID SubStreamID);

    // For the given SubStream, DeviceID and window HWND handle, boot up and setup a capture stream
    // Populates the StreamLookup Registries
    OmniStreamer::StreamID
    StartWindowCaptureStream(SubStreamID SubStreamID, DeviceMap DeviceID, HWND Hwnd);

    // Removes window capture stream, cleans up stream lookup mappings
    void StopWindowCaptureStream(SubStreamID SubStreamID);

    // Base function to create an async WinForge stream window.
    StreamWindow*
    CreateStreamWindow(const WindowCreationData& WindowData, int ShowCmd = SW_SHOWNORMAL);

    // Handles the OnClose event of a stream window, closes the sub stream, destroys the window.
    // FYI it's delegating work to a thread to let the window proc msg end to safely deconstruct.
    void OnStreamWindowClose(SubStreamID WindowKey, DeviceMap DeviceID);

    // Processes Window Drag Events received from the source stream
    // On Begin starts up a Stream Window and configure the substream with the pool.
    // On Move looks up the correct window and sets position
    // On Drag looks up the correct window, sets the position, handles dimension change and focus
    // On Cancel Destroys the Stream Window created on begin and releases the sub stream
    void
    HandleStreamWindowDrag(const OmniWinDragPacket& Packet, DeviceMap SenderDevice = DeviceMap::C0);

    // Processes Window Resize Events for both ends, simply looks up the correct HWND and resizes
    void HandleStreamWindowResize(const OmniWinResizePacket Packet);

    // Creates a Stream Window to receive a capture stream over the network.
    // Additionally sets up window event handling such as OnInput, OnResize and OnWindowClose.
    // Returns the StreamWindow's populated frame buffer PoolConfig
    // Frame bytes are expected to be written to the pool after which gets decoded and rendered
    OmniNet::PoolConfig SetupStreamRenderWindow(
        SubStreamID      SubStreamID,
        DeviceMap        DeviceID,
        uint32_t         Width   = 0,
        uint32_t         Height  = 0,
        int16_t          X       = 0,
        int16_t          Y       = 0,
        int              ShowCmd = SW_SHOW,
        std::string_view Title   = "Stream Window"
    );

    void DestroyStreamRenderWindow(SubStreamID SubStreamID);

    OmniNet::PoolConfig GetStreamWindowPoolConfig(SubStreamID SubStreamID);

    // Callback for WindowLink stream source window
    // Called upon a DragLink trigger when an edge crossing drag event begins, Drops, Cancels
    // Although move is handled here it's not called, unused for now
    SubStreamID HandleWindowDragEvent(HWND Hwnd, DeviceMap TargetDevice, WinDragAction Action);

    // Toggles edge crossing detection for the cursor.
    void ToggleEdgeProbe();

    // Registers/Unregisters a device in the OmniRouter for InputLink
    void BindIOLinkSession(DeviceMap DeviceID);
    void UnbindIOLinkSession(DeviceMap DeviceID);

    // Blocks input pass throught to the callers device, but I did include a breakout
    void ToggleInputFilter();

    OmniNet::PoolConfig SetScreenLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    OmniNet::PoolConfig SetWindowLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    // Register/Unregister the edge cross condition for this device and bind/Unbind the net session.
    // Setup Edge Probe and Input Shields if not active.. or... remove.
    OmniNet::PoolConfig SetInputLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    OmniNet::PoolConfig SetAudioLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    // By default ClipboardLink is a Duplex route feature, meaning toggling is simple.
    // Uses bitmasking for toggling, stops monitoring if no instance is subscribed to the clipboard
    // On deactivation additionally ends all the tcp streams in ActiveClipboardStreams
    // Activation requires TransmitClipboard, TransmitClipboardManifest, and ReceiveClipboardData
    // callbacks to be set.
    OmniNet::PoolConfig SetClipboardLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    void TransmitClipboard(const std::string& Text);
    void TransmitClipboardManifest(const ClipboardManifest& Manifest);
};
