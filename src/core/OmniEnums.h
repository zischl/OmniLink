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

enum NetLinkState : uint8_t {
    INACTIVE = 0,     // Air... Nothing...
    FAILED = 1,       // Failed Air.. just like unemployed me
    LINKING_INIT = 2, // OmniNetSession Creation Complete, About to send ConnectionRequest and State
    LINKING_WAIT = 3, // OmniNetSession Creation Complete, Waiting for handshake
    LINKING_ACK = 4,  // Requesting Handshake, Waiting for LINKING_AUTH or FAILED
    LINKING_AUTH = 5, // ECDH Derived, awaiting confirmation..
    LINKED = 6        // HandshakeData verified, Complete.. congrats, no longer air
};

enum DeviceMap : uint8_t { C0, L1, U1, R1, D1, LU1, RU1, RD1, LD1, END };

enum CoreCommands { OmniStatus, ScanInstances };

enum CoreCommandsWArgs : uint8_t {
    InitiateHandshake,
    ConnectDevice,

    AuthlessGate,

    SwapLayout = AuthlessGate,
    CreateStreamLink
};

#endif // OMNIENUMS_H
