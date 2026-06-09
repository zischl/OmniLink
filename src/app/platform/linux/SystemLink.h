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

struct SystemLink
{
    RenderState& render;
    CaptureController& capture;
    IOLink& input;
    std::vector<StreamWindow*>& activeWindows;

    NetworkPacketHandlerFn networkPacketHandler = nullptr;

    SystemLink(RenderState& renderState,
               CaptureController& captureCtrl,
               IOLink& inputLink,
               std::vector<StreamWindow*>& windows);

    StreamWindow* CreateStreamWindow(const WindowCreationData& info);

    void ToggleEdgeProbe();

    void SyncInputFilter();

    CaptureController::StreamID AddCaptureStream(session* netSession,
                                                 DeviceMap targetID,
                                                 CaptureMode mode);
};

#endif
