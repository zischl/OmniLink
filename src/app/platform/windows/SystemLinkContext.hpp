#pragma once

#include "OmniEnums.hpp"
#include "OmniInstances.hpp"
#include "OmniPackets.hpp"
#include "SessionTypes.hpp"

#include <Windows.h>
#include <cstdint>
#include <functional>

class OmniRouter;
class OmniSpatialRouter;
struct OmniGraphicsContext;
struct SystemStreamSessions;

// System context for all feature links
struct SystemLinkContext
{
    HWND                     WindowID        = nullptr;
    HINSTANCE                hInstance       = nullptr;
    ActiveInstanceContainer* ActiveInstances = nullptr;
    OmniRouter&              Router;
    OmniSpatialRouter&       SpatialRouter;
    OmniGraphicsContext&     GraphicsContext;
    SystemStreamSessions&    StreamSessions;

    std::function<SubStreamID(DeviceMap, FeatureTypes)>&                     RequestSubStream;
    std::function<void(DeviceMap, SubStreamID, bool)>&                       ReleaseSubStream;
    std::function<void(DeviceMap, SubStreamID, const OmniNet::PoolConfig&)>& ConfigureSubStream;
};
