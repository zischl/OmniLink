#pragma once

#include "CaptureController.h"
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
#include <utility>
#include <vector>

#include <Windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

using NetworkPacketHandlerFn = void(char*, uint32_t, uint8_t, void*);

NetworkPacketHandlerFn NetworkPacketHandler;

struct OmniSystemLink
{
    IOLinkContext IOCtx;
    OmniIOCap IOCapture{IOCtx};
    OmniIOShield IOShield{IOCtx};
    OmniStreamController StreamController;

    ComPtr<ID3D11Device> StreamingDevice = nullptr;
    ComPtr<ID3D11DeviceContext> StreamingContext = nullptr;

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

    void SetupSystemLink(HINSTANCE hInstance, int nCmdShow, HWND WindowID);

    StreamWindow* CreateStreamWindow(const WindowCreationData& WindowData);

    void ToggleEdgeProbe();

    void BindIOLinkSession(DeviceMap DeviceID);
    void UnbindIOLinkSession(DeviceMap DeviceID);

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
