#ifndef OMNIINSTANCES_H
#define OMNIINSTANCES_H

#pragma once
#include "OmniEnums.h"
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <windows.h>

#if defined(_WIN32)
#define OmniDevNameLen MAX_COMPUTERNAME_LENGTH
#elif defined(__linux__)
#define OmniDevNameLen 16
#else
#define OmniDevNameLen 16
#endif

class session;
class WinForge;

struct OmniIP {
  uint32_t InstanceIP = 0;
  char IPv4_String[16] = {};
};

struct OmniInstance {
  char InstanceName[OmniDevNameLen + 1] = {};
  uint32_t InstanceIP = 0;
  char IPv4_String[16] = {};
  uint8_t DevMapIndex = 0;

  OmniInstance() {}

  OmniInstance(uint8_t DevMIndex) { DevMapIndex = DevMIndex; }

  void Clear() {
    memset(InstanceName, 0, sizeof(InstanceName));
    InstanceIP = 0;
    memset(IPv4_String, 0, sizeof(IPv4_String));
  }

  void Edit(char *InstanceName_, char *IPv4_String_, uint32_t InstanceIP_) {
    InstanceIP = InstanceIP_;
    strncpy(IPv4_String, IPv4_String_, 16);
    strncpy(InstanceName, InstanceName_, (OmniDevNameLen + 1));
  }
};

struct OmniActiveInstance : OmniInstance {
  session *InstanceSession = nullptr;
  uint16_t port = 62485;
  int ActiveFlags = FeatureFlags::fInactive;

  OmniActiveInstance() {}

  OmniActiveInstance(char *InstanceName_, char *IPv4_String_,
                     uint32_t InstanceIP_) {
    InstanceIP = InstanceIP_;
    strncpy(IPv4_String, IPv4_String_, 16);
    strncpy(InstanceName, InstanceName_, (OmniDevNameLen + 1));
  }
};

struct PacketHandlerContext {
  WinForge *ActiveWindow;
};

using ActiveInstanceContainer =
    std::unordered_map<DeviceMap, OmniActiveInstance>;

template <size_t MaxFrameLen> struct FrameByte {
  char Frame[MaxFrameLen];
  size_t FrameLen = 0;

  FrameByte(size_t frame_len) { FrameLen = frame_len; }
};

#endif // OMNIINSTANCES_H
