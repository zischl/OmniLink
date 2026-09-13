#pragma once
#if defined(_WIN32)
  #include "platform/windows/SessionHandler.hpp"
#elif defined(__linux__)
  #include "platform/linux/SessionHandler.hpp"
#endif
