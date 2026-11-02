#pragma once

#include "OmniEnums.hpp"
#include "OmniInstances.h"
#include "OmniPackets.hpp"
#include "SessionTypes.hpp"

#include <Windows.h>
#include <cstdint>
#include <functional>

class OmniRouter;
struct OmniGraphicsContext;
struct SystemCaptureStreams;

using SubStreamID = uint16_t;

// System context for all feature links
struct SystemLinkContext
{
    HWND                     WindowID        = nullptr;
    HINSTANCE                hInstance       = nullptr;
    ActiveInstanceContainer* ActiveInstances = nullptr;
    OmniRouter&              Router;
    OmniGraphicsContext&     GraphicsContext;
    SystemCaptureStreams&    CaptureStreams;

    std::function<SubStreamID(DeviceMap, FeatureTypes)>&                     RequestSubStream;
    std::function<void(DeviceMap, SubStreamID, bool)>&                       ReleaseSubStream;
    std::function<void(DeviceMap, SubStreamID, const OmniNet::PoolConfig&)>& ConfigureSubStream;
};
