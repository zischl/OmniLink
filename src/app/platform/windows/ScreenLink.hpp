#pragma once

#include "CaptureController.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SystemLinkContext.hpp"

struct SystemScreenLink
{
    // For the given SubStream and DeviceID, boot up and setup a DXGI screen capture stream
    // Populates the StreamLookup Registries
    OmniStreamer::StreamID
    StartScreenCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID, DeviceMap DeviceID);

    // Removes screen capture stream, cleans up stream lookup mappings
    void StopScreenCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID);

    OmniNet::PoolConfig SetState(
        SystemLinkContext& Ctx,
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );
};
