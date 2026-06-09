#pragma once

#if defined(_WIN32)
#include "platform/windows/SystemLink.h"
#elif defined(__linux__)
#include "platform/linux/SystemLink.h"
#endif
