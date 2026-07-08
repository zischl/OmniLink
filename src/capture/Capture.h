#pragma once
#if defined(_WIN32)
#include "platform/windows/WinCap.h"
using ScreenCaptureDXGI = DXGICapture;
using ScreenCaptureWGC = WGScreenCaptureEx;
#elif defined(__linux__)
#include "platform/linux/PipeWireCap.h"
using ScreenCapturePW = PipeWireCapture;
#endif
