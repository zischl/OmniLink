#pragma once
#include <cstdint>

enum FrameAquisition { EventDriven, Polling };

enum class CaptureAPI { DXGI, WGC, PipeWire, Mock };

struct StreamConfig
{
    uint32_t Width = 1920;
    uint32_t Height = 1080;
};
