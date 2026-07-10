#ifndef SYSTEMLINK_H
#define SYSTEMLINK_H

#pragma once

#include "CaptureController.h"
#include "IOLink.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "RenderState.h"
#include "StreamWindow.h"

#include <vector>

class session;

using NetworkPacketHandlerFn = void (*)(char*, uint32_t, uint8_t, void*);

struct OmniSystemLink
{
    RenderState& render;
    CaptureController& capture;
    IOLink& input;
    std::vector<StreamWindow*> ActiveWindows;

    NetworkPacketHandlerFn networkPacketHandler = nullptr;

    OmniSystemLink(RenderState& renderState,
                   CaptureController& captureCtrl,
                   IOLink& inputLink);

    StreamWindow* CreateStreamWindow(const WindowCreationData& info);

    void ToggleEdgeProbe();

    void SyncInputFilter();

    CaptureController::StreamID AddCaptureStream(session* netSession,
                                                 DeviceMap targetID,
                                                 CaptureMode mode);
};

#endif
