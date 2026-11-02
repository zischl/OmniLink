#pragma once

#include "ClipboardTypes.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "OmniTCPStream.h"
#include "SystemLinkContext.h"
#include "clipboard/platform/windows/ClipBoardLink.h"

#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct SystemClipboardLink
{
    // Feature Class Instance for Clipboard Link Controller
    OmniClipboardLink ClipboardLink;

    // Active Instances pointer maintained from SystemLinkContext
    ActiveInstanceContainer* ActiveInstances = nullptr;

    // ClipboardLink Subscribers listed in atomic bitmask, TCP streams, event callbacks
    std::atomic<uint16_t>                                        ActiveClipboardSubscriptions{0};
    std::mutex                                                   ClipboardStreamsMutex;
    std::unordered_map<uint32_t, std::shared_ptr<OmniTCPStream>> ActiveClipboardStreams;
    std::function<void(const ClipboardStreamEvent&)>             OnClipboardStreamEvent;

    // By default ClipboardLink is a Duplex route feature, meaning toggling is simple.
    // Uses bitmasking for toggling, stops monitoring if no instance is subscribed to the clipboard
    // On deactivation additionally ends all the tcp streams in ActiveClipboardStreams
    // Activation requires TransmitClipboard, TransmitClipboardManifest, and ReceiveClipboardData
    // callbacks to be set.
    OmniNet::PoolConfig SetState(
        SystemLinkContext& Ctx,
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    void TransmitClipboard(const std::string& Text);
    void TransmitClipboardManifest(const ClipboardManifest& Manifest);
};
