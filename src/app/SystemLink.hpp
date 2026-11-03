#pragma once

#if defined(_WIN32)
#include "platform/windows/SystemLink.hpp"
#elif defined(__linux__)
#include "platform/linux/SystemLink.hpp"
#endif
