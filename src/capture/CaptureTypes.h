#pragma once
#include <cstdint>
#if defined(_WIN32)
#include <Windows.h>
#endif

enum FrameAquisition { EventDriven, Polling };

enum class CaptureAPI { DXGI, WGC, PipeWire, Mock };

struct StreamConfig
{
    uint32_t Width = 1920;
    uint32_t Height = 1080;
#if defined(_WIN32)
    HWND WindowHandle = NULL;
#endif
};

