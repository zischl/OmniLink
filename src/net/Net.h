#pragma once
#if defined(_WIN32)
  #include "platform/windows/SessionHandler.h"
#elif defined(__linux__)
  #include "platform/linux/SessionHandler.h"
#endif
