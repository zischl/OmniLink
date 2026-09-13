#pragma once
#if defined(_WIN32)
  #include "platform/windows/WinForge.hpp"
#elif defined(__linux__)
  #include "platform/linux/LinuxForge.hpp"
#endif
