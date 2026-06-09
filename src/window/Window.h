#pragma once
#if defined(_WIN32)
  #include "platform/windows/WinForge.h"
#elif defined(__linux__)
  #include "platform/linux/LinuxForge.h"
#endif
