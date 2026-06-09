#ifndef SYSTEMLINK_H
#define SYSTEMLINK_H

#pragma once

#include "CaptureController.h"
#include "IOLink.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "RenderState.h"
#include "StreamWindow.h"

#include <Windows.h>
#include <vector>

class session;

using NetworkPacketHandlerFn = void (*)(CHAR*, DWORD, uint8_t, void*);

struct OmniSystemLink
{
    OmniStreamController StreamController;
    OmniIOCap IOCapture;
    OmniIOShield IOShield;

    OmniRenderState& RenderState;
    std::vector<StreamWindow*>& ActiveWindows;

    HINSTANCE hInstance = nullptr;
    int nCmdShow = 0;
    HWND WindowID = nullptr;

    NetworkPacketHandlerFn networkPacketHandler = nullptr;

    OmniSystemLink(OmniRenderState& RenderState, std::vector<StreamWindow*>& ActiveWindows);

    void SetupSystemLink(HINSTANCE hInstance, int nCmdShow, HWND WindowID);

    StreamWindow* CreateStreamWindow(const WindowCreationData& WindowData);

    void ToggleEdgeProbe(ActiveInstanceContainer& ActiveInstances);

    void SyncInputFilter();

    OmniStreamController::StreamID
    AddCaptureStream(session* netSession, DeviceMap targetID, CaptureMode mode);
};

#endif
