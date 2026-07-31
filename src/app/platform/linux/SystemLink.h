#ifndef SYSTEMLINK_H
#define SYSTEMLINK_H

#pragma once

#include "CaptureController.h"
#include "IOLink.h"
#include "OmniEnums.h"
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
    OmniIOCap IOCapture;
    OmniIOShield IOShield;

    void* StreamingDevice = nullptr;
    void* StreamingContext = nullptr;

    OmniRenderState& RenderState;
    std::vector<StreamWindow*> ActiveWindows;
    std::multimap<std::pair<DeviceMap, FeatureTypes>, StreamWindow*> WindowRegistry;
    std::multimap<std::pair<DeviceMap, FeatureTypes>, OmniStreamController::StreamID>
        StreamRegistry;

    HINSTANCE hInstance = nullptr;
    int nCmdShow = 0;
    HWND WindowID = nullptr;
    ActiveInstanceContainer* ActiveInstances = nullptr;

    OmniSystemLink(OmniRenderState& RenderState);

    void SetupSystemLink();

    StreamWindow* CreateStreamWindow(const WindowCreationData& WindowData);

    void ToggleEdgeProbe(ActiveInstanceContainer& ActiveInstances);

    void SyncInputFilter();

    OmniStreamController::StreamID
    AddCaptureStream(OmniNetSubStream* SubStream, DeviceMap DeviceID, CaptureMode Mode);

    OmniNet::PoolConfig
    SetScreenLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);

    OmniNet::PoolConfig
    SetWindowLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);

    OmniNet::PoolConfig
    SetInputLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);

    OmniNet::PoolConfig
    SetAudioLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);

    OmniNet::PoolConfig
    SetClipboardLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);
};

#endif
