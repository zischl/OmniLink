#ifndef SYSTEMLINK_H
#define SYSTEMLINK_H

#include <cstdint>
#pragma once

#include "CaptureController.h"
#include "IOLink.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "RenderState.h"
#include "StreamWindow.h"

#include <vector>

class session;

using NetworkPacketHandlerFn = void(char*, uint32_t, uint8_t, void*);

NetworkPacketHandlerFn NetworkPacketHandler;

struct OmniSystemLink
{
    RenderState& render;
    CaptureController& capture;
    IOLink& input;
    std::vector<StreamWindow*> ActiveWindows;

    OmniSystemLink(RenderState& renderState, CaptureController& captureCtrl, IOLink& inputLink);

    StreamWindow* CreateStreamWindow(const WindowCreationData& info);

    void ToggleEdgeProbe();

    void SyncInputFilter();

    CaptureController::StreamID
    AddCaptureStream(session* netSession, DeviceMap targetID, CaptureMode mode);
};

#endif
