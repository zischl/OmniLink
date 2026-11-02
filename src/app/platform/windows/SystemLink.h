#pragma once

#include "AudioLink.h"
#include "CaptureStreams.h"
#include "ClipboardLink.h"
#include "InputLink.h"
#include "OmniEnums.hpp"
#include "OmniGraphicsContext.hpp"
#include "OmniInstances.h"
#include "OmniPackets.hpp"
#include "OmniRouterContext.hpp"
#include "ScreenLink.h"
#include "SpatialRouter.hpp"
#include "SystemLinkContext.h"
#include "WindowLink.h"

#include <Windows.h>
#include <algorithm>
#include <climits>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

using NetworkPacketHandlerFn = void(char*, uint32_t, uint8_t, void*);

NetworkPacketHandlerFn NetworkPacketHandler;

struct OmniSystemLink
{
    // Main Window Data
    HINSTANCE hInstance = nullptr;
    int       nCmdShow  = 0;
    HWND      WindowID  = nullptr;

    // Active Instances borrowed from InstanceRegistry
    ActiveInstanceContainer* ActiveInstances = nullptr;

    // Core Router & Graphics Context
    OmniRouter           OmniRouter;
    OmniGraphicsContext& GraphicsContext;

    // SubStream request, release, and configuration callbacks.
    std::function<SubStreamID(DeviceMap, FeatureTypes)>                     RequestSubStream;
    std::function<void(DeviceMap, SubStreamID, bool)>                       ReleaseSubStream;
    std::function<void(DeviceMap, SubStreamID, const OmniNet::PoolConfig&)> ConfigureSubStream;

    // Composed Feature Modules
    OmniSpatialRouter    SpatialRouter;
    SystemCaptureStreams CaptureStreams;
    SystemAudioLink      AudioLinkModule;
    SystemClipboardLink  ClipboardLinkModule;
    SystemInputLink      InputLinkModule{OmniRouter, SpatialRouter};
    SystemScreenLink     ScreenLinkModule;
    SystemWindowLink     WindowLinkModule;

    OmniSystemLink(OmniGraphicsContext& GraphicsContext);
    ~OmniSystemLink();

    void SetupSystemLink(HINSTANCE hInstance, int nCmdShow, HWND WindowID);

    inline OmniSpatialRouter& GetSpatialRouter() { return SpatialRouter; }

    inline SystemLinkContext GetContext()
    {
        return SystemLinkContext{
            WindowID,
            hInstance,
            ActiveInstances,
            OmniRouter,
            SpatialRouter,
            GraphicsContext,
            CaptureStreams,
            RequestSubStream,
            ReleaseSubStream,
            ConfigureSubStream
        };
    }

    // Feature Lifecycle Handlers

    inline OmniNet::PoolConfig SetScreenLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    )
    {
        auto Ctx = GetContext();
        return ScreenLinkModule.SetState(Ctx, DeviceID, Route, Action, SubStreamID, Context);
    }

    inline OmniNet::PoolConfig SetWindowLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    )
    {
        auto Ctx = GetContext();
        return WindowLinkModule.SetState(Ctx, DeviceID, Route, Action, SubStreamID, Context);
    }

    inline OmniNet::PoolConfig SetInputLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    )
    {
        auto Ctx = GetContext();
        return InputLinkModule.SetState(Ctx, DeviceID, Route, Action, SubStreamID, Context);
    }

    inline OmniNet::PoolConfig SetAudioLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    )
    {
        auto Ctx = GetContext();
        return AudioLinkModule.SetState(Ctx, DeviceID, Route, Action, SubStreamID, Context);
    }

    inline OmniNet::PoolConfig SetClipboardLinkState(
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    )
    {
        auto Ctx = GetContext();
        return ClipboardLinkModule.SetState(Ctx, DeviceID, Route, Action, SubStreamID, Context);
    }

    /**
     * @brief Below are the Inlined Forwarders for External Callers.
     *
     * No indirections or function call overhead, why?
     * The memory layout of SystemLink and inline substitution should guarantee this.
     */

    inline void ToggleEdgeProbe() { InputLinkModule.ToggleEdgeProbe(); }
    inline void ToggleInputFilter() { InputLinkModule.ToggleInputFilter(); }
    inline void BindIOLinkSession(DeviceMap DeviceID)
    {
        auto Ctx = GetContext();
        InputLinkModule.BindIOLinkSession(Ctx, DeviceID);
    }
    inline void UnbindIOLinkSession(DeviceMap DeviceID)
    {
        auto Ctx = GetContext();
        InputLinkModule.UnbindIOLinkSession(Ctx, DeviceID);
    }

    inline void TransmitClipboard(const std::string& Text)
    {
        ClipboardLinkModule.TransmitClipboard(Text);
    }
    inline void TransmitClipboardManifest(const ClipboardManifest& Manifest)
    {
        ClipboardLinkModule.TransmitClipboardManifest(Manifest);
    }

    inline std::vector<uint8_t>
    ReceiveClipboardData(const ClipboardManifest& Manifest, UINT Format = 0)
    {
        return ClipboardLinkModule.ReceiveClipboardData(Manifest, Format);
    }

    inline void
    HandleStreamWindowDrag(const OmniWinDragPacket& Packet, DeviceMap DeviceID = DeviceMap::C0)
    {
        auto Ctx = GetContext();
        WindowLinkModule.HandleStreamWindowDrag(Ctx, Packet, DeviceID);
    }

    void HandleStreamWindowResize(const OmniWinResizePacket Packet);
    void HandleStreamWindowMouse(const OmniWinMousePacket& Packet);
    void HandleStreamWindowKey(const OmniWinKeyPacket& Packet);

    inline void HandleStreamControlPacket(
        const OmniStreamControlPacket& Packet, DeviceMap SenderDevice = DeviceMap::C0
    )
    {
        auto Ctx = GetContext();
        CaptureStreams.HandleStreamControlPacket(Ctx, Packet, SenderDevice);
    }

    inline OmniNet::PoolConfig SetupStreamRenderWindow(
        SubStreamID      SubStreamID,
        DeviceMap        DeviceID,
        uint32_t         Width   = 0,
        uint32_t         Height  = 0,
        int16_t          X       = 0,
        int16_t          Y       = 0,
        int              ShowCmd = SW_SHOW,
        std::string_view Title   = "Stream Window"
    )
    {
        auto Ctx = GetContext();
        return CaptureStreams.SetupStreamRenderWindow(
            Ctx, SubStreamID, DeviceID, Width, Height, X, Y, ShowCmd, Title
        );
    }

    inline void DestroyStreamRenderWindow(SubStreamID SubStreamID)
    {
        CaptureStreams.DestroyStreamRenderWindow(SubStreamID);
    }

    inline OmniNet::PoolConfig GetStreamWindowPoolConfig(SubStreamID SubStreamID)
    {
        return CaptureStreams.GetStreamWindowPoolConfig(SubStreamID);
    }

    inline bool RecallWindow(HWND SourceHwnd, bool CenterOnDesktop = true)
    {
        auto Ctx = GetContext();
        return WindowLinkModule.RecallWindow(Ctx, SourceHwnd, CenterOnDesktop);
    }

    inline size_t RecallAllStreamingWindows()
    {
        auto Ctx = GetContext();
        return WindowLinkModule.RecallAllStreamingWindows(Ctx);
    }

    inline void CenterWindowOnDesktop(HWND TargetHwnd)
    {
        WindowLinkModule.CenterWindowOnDesktop(TargetHwnd);
    }
};
