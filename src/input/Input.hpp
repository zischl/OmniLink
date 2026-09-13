#pragma once
#if defined(_WIN32)
  #include "platform/windows/IOLink.hpp"
#elif defined(__linux__)
  #include "platform/linux/IOLink.hpp"
#endif
