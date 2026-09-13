#pragma once
#if defined(_WIN32)
#include "platform/windows/WinCap.hpp"
using ScreenCaptureDXGI = DXGICapture;
using ScreenCaptureWGC = WGScreenCaptureEx;
using WindowCaptureWGC = WGWindowCaptureEx;
#elif defined(__linux__)
#include "platform/linux/PipeWireCap.hpp"
using ScreenCapturePW = PipeWireCapture;
#endif
