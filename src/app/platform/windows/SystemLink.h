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

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
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
    std::vector<StreamWindow*>                              ActiveWindows;
    std::unordered_map<SubStreamID, StreamWindow*>          StreamWindowRegistry;
    std::unordered_map<SubStreamID, OmniStreamer::StreamID> StreamerIDRegistry;

    // Map/Reverse Map Hwnd with Sub Streams for the DragDetection in WindowLink
    std::unordered_map<HWND, SubStreamID>      Hwnd2SubStreamRegistry;
    std::unordered_map<SubStreamID, HWND>      SubStream2HwndRegistry;
    std::unordered_map<SubStreamID, DeviceMap> SubStreamToDevice;

    struct WindowStreamContext
    {
        OmniSystemLink*          SysLink   = nullptr;
        OmniNetSession<OmniMTU>* Session   = nullptr;
        DeviceMap                DeviceID  = DeviceMap::C0;
        SubStreamID              WindowKey = 0;
    };

    std::unordered_map<SubStreamID, WindowStreamContext> StreamContexts;

    // Callbacks on Streamer Window Open and Close
    std::function<SubStreamID(DeviceMap, HWND)> OnOpenWindowStream;
    std::function<void(DeviceMap, SubStreamID)> OnCloseWindowStream;

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

    void SetupSystemLink(HINSTANCE hInstance, int nCmdShow, HWND WindowID);

    StreamWindow*
    CreateStreamWindow(const WindowCreationData& WindowData, int ShowCmd = SW_SHOWNORMAL);

    void ToggleEdgeProbe();

    void BindIOLinkSession(DeviceMap DeviceID);
    void UnbindIOLinkSession(DeviceMap DeviceID);

    void SyncInputFilter();

    OmniStreamer::StreamID AddCaptureStream(
        OmniNetSubStream*   SubStream,
        DeviceMap           DeviceID,
        CaptureMode         Mode,
        const StreamConfig& Config = {}
    );

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
