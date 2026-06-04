#pragma once
#if defined(_WIN32)
  #include "platform/windows/IOLink.h"
#elif defined(__linux__)
  #include "platform/linux/IOLink.h"
#endif
