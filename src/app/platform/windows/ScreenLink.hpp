#pragma once

#include "CaptureController.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SystemLinkContext.hpp"
#include "WindowOperationTypes.hpp"

#include <Windows.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

template <uint32_t MTU> class OmniNetSession;

// ScreenLink client session for storing session data while it lasts.
// Only stored on the client side, the receiving end of the stream.
// WindowHwnd is the stream window's handle.
struct ScreenStreamEntry
{
    OmniNetSession<OmniMTU>*                          NetSession = nullptr;
    std::function<void(DeviceMap, SubStreamID, bool)> ReleaseSubStream;
    DeviceMap                                         DeviceID   = DeviceMap::C0;
    SubStreamID                                       StreamID   = 0;
    HWND                                              WindowHwnd = NULL;
};

struct SystemScreenLink
{
    // Screen link client side active sessions
    std::unordered_map<SubStreamID, std::unique_ptr<ScreenStreamEntry>> ScreenStreamEntries;

    // For the given SubStream and DeviceID, boot up and setup a DXGI screen capture stream
    // Populates the StreamLookup Registries
    OmniStreamer::StreamID
    StartScreenCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID, DeviceMap DeviceID);

    // Removes screen capture stream, cleans up stream lookup mappings
    void StopScreenCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID);

    // Inbound network input handler on the host device
    // Relays client stream window mouse movements and button events to Host desktop
    void HandleScreenMouse(const OmniWinMousePacket& Packet);

    // Inbound network input handler on the host device
    // Relays client stream window keyboard events to Host desktop
    void HandleScreenKey(const OmniWinKeyPacket& Packet);

    // Sets up the StreamWindow for receiving screen frames on the Client
    // And... hooks up KVM forwarders
    OmniNet::PoolConfig SetupClientStreamRenderWindow(
        SystemLinkContext& Ctx,
        SubStreamID        SubStreamID,
        DeviceMap          DeviceID,
        uint32_t           Width,
        uint32_t           Height
    );

    OmniNet::PoolConfig SetState(
        SystemLinkContext& Ctx,
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );
};
