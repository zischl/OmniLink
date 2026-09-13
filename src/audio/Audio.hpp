#pragma once

#if defined(_WIN32)
#include "platform/windows/AudioCap.hpp"
#include "platform/windows/AudioRender.hpp"
#elif defined(__linux__)
// Linux audio platform headers to be.. added.. later
#endif
