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

#include <Windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

template <uint32_t MTU> class OmniNetSession;

using NetworkPacketHandlerFn = void(char*, uint32_t, uint8_t, void*);

NetworkPacketHandlerFn NetworkPacketHandler;

struct OmniSystemLink
{
    OmniStreamController StreamController;
    OmniIOCap IOCapture;
    OmniIOShield IOShield;

    ComPtr<ID3D11Device> StreamingDevice = nullptr;
    ComPtr<ID3D11DeviceContext> StreamingContext = nullptr;

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
};

#endif
