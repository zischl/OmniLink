#pragma once

#include "AudioCap.h"
#include "AudioRender.h"
#include "CaptureController.h"
#include "ClipBoardLink.h"
#include "ClipboardTypes.h"
#include "IOLink.h"
#include "IOLinkContext.h"
#include "OmniEnums.h"
#include "OmniInstances.h"
#include "OmniPackets.h"
#include "RenderState.h"
#include "StreamWindow.h"

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

NetworkPacketHandlerFn NetworkPacketHandler;

struct OmniSystemLink
{
    IOLinkContext                                    IOCtx;
    OmniIOCap                                        IOCapture{IOCtx};
    OmniIOShield                                     IOShield{IOCtx};
    OmniStreamController                             StreamController;
    ClipBoardLink                                    ClipboardService;
    std::unique_ptr<AudioCapture>                    OmniAudioCapture = nullptr;
    std::map<uint16_t, OmniNetSubStream*>            ActiveAudioStreams;
    std::mutex                                       AudioBroadcastMutex;
    std::map<uint16_t, std::unique_ptr<AudioRender>> AudioRenderers;

    ComPtr<ID3D11Device>        StreamingDevice  = nullptr;
    ComPtr<ID3D11DeviceContext> StreamingContext = nullptr;

    OmniRenderState&                                             RenderState;
    std::vector<StreamWindow*>                                   ActiveWindows;
    std::unordered_map<uint16_t, StreamWindow*>                  WindowRegistry;
    std::unordered_map<uint16_t, OmniStreamController::StreamID> StreamRegistry;

    HINSTANCE                hInstance       = nullptr;
    int                      nCmdShow        = 0;
    HWND                     WindowID        = nullptr;
    ActiveInstanceContainer* ActiveInstances = nullptr;

    ClipboardFeatureContext* ClipboardCtx = nullptr;

    OmniSystemLink(OmniRenderState& RenderState);

    void SetupSystemLink(HINSTANCE hInstance, int nCmdShow, HWND WindowID);

    StreamWindow* CreateStreamWindow(const WindowCreationData& WindowData);

    void ToggleEdgeProbe();

    void BindIOLinkSession(DeviceMap DeviceID);
    void UnbindIOLinkSession(DeviceMap DeviceID);

    void SyncInputFilter();

    OmniStreamController::StreamID AddCaptureStream(
        OmniNetSubStream*   SubStream,
        DeviceMap           DeviceID,
        CaptureMode         Mode,
        const StreamConfig& Config = {}
    );

    OmniNet::PoolConfig SetScreenLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        uint16_t           SubStreamID = 0,
        void*              Context     = nullptr
    );

    OmniNet::PoolConfig SetWindowLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        uint16_t           SubStreamID = 0,
        void*              Context     = nullptr
    );

    OmniNet::PoolConfig SetInputLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        uint16_t           SubStreamID = 0,
        void*              Context     = nullptr
    );

    OmniNet::PoolConfig SetAudioLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        uint16_t           SubStreamID = 0,
        void*              Context     = nullptr
    );

    OmniNet::PoolConfig SetClipboardLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        uint16_t           SubStreamID = 0,
        void*              Context     = nullptr
    );

    void TransmitClipboard(const std::string& Text);
    void TransmitClipboardManifest(const ClipboardManifest& Manifest);
};
