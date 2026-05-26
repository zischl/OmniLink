#ifndef OMNIENUMS_H
#define OMNIENUMS_H

#pragma once
#include <cstdint>

enum FeatureFlags {
  fInactive = 0,
  fScreenLink = 1 << 0,
  fWindowLink = 1 << 1,
  fInputLink = 1 << 2,
  fLink = 1 << 3,
};

enum FeatureTypes { ScreenLink, WindowLink, InputLink, AudioLink };

enum DeviceMap : uint8_t { C0, L1, U1, R1, D1, LU1, RU1, RD1, LD1 };

enum CoreCommands { OmniStatus, ScanInstances };

enum CoreCommandsWArgs : uint8_t {
  SwapLayout,
  ConnectDevice,
  CreateStreamLink
};

#endif // OMNIENUMS_H
