#pragma once

#include "IOLink.hpp"
#include "IOLinkContext.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SystemLinkContext.h"

class OmniRouter;

struct SystemInputLink
{
    // Feature Class Instances for Input capture, Input filter/blocker and Input emits
    InputLinkContext InputLinkCtx;
    OmniInputLink    InputLink;
    OmniInputFilter  InputFilter;
    OmniSynth        InputSynth;

    SystemInputLink(OmniRouter& Router);

    void Setup(HWND WindowID);

    // Toggles edge crossing detection for the cursor.
    void ToggleEdgeProbe();

    // Registers/Unregisters a device in the OmniRouter for InputLink
    void BindIOLinkSession(SystemLinkContext& Ctx, DeviceMap DeviceID);
    void UnbindIOLinkSession(SystemLinkContext& Ctx, DeviceMap DeviceID);

    // Blocks input pass throught to the callers device, but I did include a breakout
    void ToggleInputFilter();

    // Register/Unregister the edge cross condition for this device and bind/Unbind the net session.
    // Setup Edge Probe and Input Shields if not active.. or... remove.
    OmniNet::PoolConfig SetState(
        SystemLinkContext& Ctx,
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );
};
