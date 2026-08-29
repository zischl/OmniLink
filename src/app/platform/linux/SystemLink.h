#ifndef SYSTEMLINK_H
#define SYSTEMLINK_H

#pragma once
#include "CaptureController.h"
#include "ClipBoardLink.h"
#include "ClipboardTypes.h"
#include "IOLink.h"
#include "OmniEnums.h"
#include "OmniInstances.h"
#include "OmniPackets.h"
#include "RenderState.h"
#include "StreamWindow.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

using NetworkPacketHandlerFn = void(char*, uint32_t, uint8_t, void*);

NetworkPacketHandlerFn NetworkPacketHandler;

struct OmniSystemLink
{
    OmniStreamController StreamController;
    OmniIOCap            IOCapture;
    OmniIOShield         IOShield;
    ClipBoardLink        ClipboardService;

    void* StreamingDevice  = nullptr;
    void* StreamingContext = nullptr;

    OmniRenderState&                                             RenderState;
    std::vector<StreamWindow*>                                   ActiveWindows;
    std::unordered_map<uint16_t, StreamWindow*>                  WindowRegistry;
    std::unordered_map<uint16_t, OmniStreamController::StreamID> StreamRegistry;

    HINSTANCE                hInstance       = nullptr;
    int                      nCmdShow        = 0;
    HWND                     WindowID        = nullptr;
    ActiveInstanceContainer* ActiveInstances = nullptr;
    ClipboardFeatureContext* ClipboardCtx    = nullptr;

    OmniSystemLink(OmniRenderState& RenderState);

    void SetupSystemLink();

    StreamWindow* CreateStreamWindow(const WindowCreationData& WindowData);

    void ToggleEdgeProbe();

    void BindSession(DeviceMap DeviceID);
    void UnbindSession(DeviceMap DeviceID);

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

#endif
