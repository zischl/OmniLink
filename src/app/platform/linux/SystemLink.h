#ifndef SYSTEMLINK_H
#define SYSTEMLINK_H

#pragma once

#include "CaptureController.h"
#include "IOLink.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "RenderState.h"
#include "StreamWindow.h"

#include <cstdint>
#include <vector>

template <uint32_t MTU> class OmniNetSession;

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

    HINSTANCE hInstance = nullptr;
    int nCmdShow = 0;
    HWND WindowID = nullptr;

    OmniSystemLink(OmniRenderState& RenderState);

    void SetupSystemLink(HINSTANCE hInstance, int nCmdShow, HWND WindowID);

    StreamWindow* CreateStreamWindow(const WindowCreationData& WindowData);

    void ToggleEdgeProbe(ActiveInstanceContainer& ActiveInstances);

    void SyncInputFilter();

    OmniStreamController::StreamID
    AddCaptureStream(OmniNetSession<OmniMTU>* netSession, DeviceMap targetID, CaptureMode mode);

    void SetScreenLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);
    void SetWindowLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);
    void SetInputLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);
    void SetAudioLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);
    void SetClipboardLinkState(DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action);
};

#endif
