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

template <uint32_t MTU> class OmniNetSession;

using NetworkPacketHandlerFn = void (*)(CHAR*, DWORD, uint8_t, void*);

struct OmniSystemLink
{
    OmniStreamController StreamController;
    OmniIOCap IOCapture;
    OmniIOShield IOShield;

    OmniRenderState& RenderState;
    std::vector<StreamWindow*> ActiveWindows;

    HINSTANCE hInstance = nullptr;
    int nCmdShow = 0;
    HWND WindowID = nullptr;

    NetworkPacketHandlerFn networkPacketHandler = nullptr;

    OmniSystemLink(OmniRenderState& RenderState);

    void SetupSystemLink(
        HINSTANCE hInstance, int nCmdShow, HWND WindowID, NetworkPacketHandlerFn PacketHandlerFn
    );

    StreamWindow* CreateStreamWindow(const WindowCreationData& WindowData);

    void ToggleEdgeProbe(ActiveInstanceContainer& ActiveInstances);

    void SyncInputFilter();

    OmniStreamController::StreamID
    AddCaptureStream(OmniNetSession<OmniMTU>* netSession, DeviceMap targetID, CaptureMode mode);
};

#endif
