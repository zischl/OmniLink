#ifndef OMNIENUMS_H
#define OMNIENUMS_H

#pragma once
#include <cstdint>

enum OmniAppState { RUNNING, STOPPING };

enum OmniGUIState { RENDER, IDLE, MINIMIZED, TRAY };

enum FeatureFlags {
    fInactive = 0,
    fScreenLink = 1 << 0,
    fWindowLink = 1 << 1,
    fInputLink = 1 << 2,
    fAudioLink = 1 << 3,
    fClipBoardLink = 1 << 4
};

enum FeatureTypes : uint8_t { ScreenLink, WindowLink, InputLink, AudioLink, ClipboardLink };

enum NetLinkState : uint8_t { INACTIVE, FAILED, WAITING, LINKING, LINKED };

enum DeviceMap : uint8_t { C0, L1, U1, R1, D1, LU1, RU1, RD1, LD1, END };

enum CoreCommands { OmniStatus, ScanInstances };

enum CoreCommandsWArgs : uint8_t { SwapLayout, ConnectDevice, CreateStreamLink, InitiateHandshake };

#endif // OMNIENUMS_H
