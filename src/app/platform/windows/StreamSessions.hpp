#pragma once

#include "CaptureController.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "StreamControlPackets.hpp"
#include "StreamWindow.hpp"
#include "SystemLinkContext.hpp"
#include "WinForge.hpp"
#include "WindowOperationTypes.hpp"

#include <Windows.h>
#include <climits>
#include <cstdint>
#include <d3d11.h>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct OmniSystemLink;
template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

// Role of a window and the session.
// Source means a real window that is streamed to a stream window on a remote device
// Inbound packets from a stream window affect the stream source.
// Inbound packets from a stream source affect the stream render window.
enum class SessionRole : uint8_t {
    None = 0,
    Source,
    Stream

};

// Structure used by both source stream sessions and stream window sessions
// Source owns SourceHwnd, Stream owns RenderWindow, RenderHwnd, rest is shared
// Meaning each session on a host has a relative client session for a stream window
// SourceHwnd can be either null or an actual hwnd of window based on feature type
// Context can be addtional data pointers needed for the feature types
struct alignas(32) StreamSessionEntry
{
    SessionRole            Role            = SessionRole::None;
    FeatureTypes           Feature         = FeatureTypes::WindowLink;
    DeviceMap              DeviceID        = DeviceMap::C0;
    HWND                   SourceHwnd      = nullptr;
    OmniStreamer::StreamID CaptureStreamID = 0;
    StreamWindow*          RenderWindow    = nullptr;
    HWND                   RenderHwnd      = nullptr;
    void*                  FeatureContext  = nullptr;
};

struct SystemStreamSessions
{
    // Feature Class Instance for CaptureStream controller
    OmniStreamer Streamer;

    // D3D Device and Context for Capture Streams
    ComPtr<ID3D11Device>        StreamingDevice  = nullptr;
    ComPtr<ID3D11DeviceContext> StreamingContext = nullptr;

    ~SystemStreamSessions();

    // Accessors for Stream Route Registry
    const StreamSessionEntry* GetSession(SubStreamID ID) const;
    StreamSessionEntry*       GetSession(SubStreamID ID);
    bool                      IsSourceWindow(SubStreamID ID) const;
    bool                      IsStreamWindow(SubStreamID ID) const;

    // Stores a stream source session in SessionRegistry.
    void RegisterSourceSession(
        SubStreamID            ID,
        DeviceMap              DeviceID,
        FeatureTypes           Feature,
        OmniStreamer::StreamID StreamID,
        HWND                   SourceHwnd = nullptr
    );

    inline void RegisterSourceRoute(
        SubStreamID            ID,
        DeviceMap              DeviceID,
        FeatureTypes           Feature,
        OmniStreamer::StreamID StreamID,
        HWND                   SourceHwnd = nullptr
    )
    {
        RegisterSourceSession(ID, DeviceID, Feature, StreamID, SourceHwnd);
    }

    // Stores a stream client window session in SessionRegistry.
    void RegisterStreamSession(
        SubStreamID   ID,
        DeviceMap     DeviceID,
        FeatureTypes  Feature,
        StreamWindow* Window,
        void*         FeatureContext = nullptr
    );

    // Unregisters.. whatever the session based on the sub stream id
    // Don't care about session type
    void UnregisterSession(SubStreamID ID);

    // Accessors for Registry Containers, self explanatory yeah ?
    StreamWindow*          GetStreamWindow(SubStreamID ID) const;
    inline StreamWindow*   FindStreamWindow(SubStreamID ID) const { return GetStreamWindow(ID); }
    OmniStreamer::StreamID GetStreamerID(SubStreamID ID) const;
    inline OmniStreamer::StreamID FindStreamerID(SubStreamID ID) const { return GetStreamerID(ID); }
    bool                          ContainsStreamWindow(SubStreamID ID) const;

    // Host-side capture lifecycle
    OmniStreamer::StreamID AddCaptureStream(
        OmniGraphicsContext& GraphicsContext,
        OmniNetSubStream*    SubStream,
        DeviceMap            DeviceID,
        CaptureMode          Mode,
        const StreamConfig&  Config = {}
    );

    void DestroyCaptureStream(SubStreamID ID);

    // Processes stream control packets received from the peer
    void HandleStreamControlPacket(
        SystemLinkContext&             Ctx,
        const OmniStreamControlPacket& Packet,
        DeviceMap                      SenderDevice = DeviceMap::C0
    );

    // Resizes a Client stream render window to match dimensions requested by the Host
    void ResizeStreamWindow(SubStreamID SubStreamID, uint32_t Width, uint32_t Height);

    // Creates a Stream Window to receive a capture stream over the network.
    // Additionally sets up window event handling such as OnInput, OnResize and OnWindowClose.
    // Returns the StreamWindow's populated frame buffer PoolConfig
    // Frame bytes are expected to be written to the pool after which gets decoded and rendered
    OmniNet::PoolConfig SetupStreamRenderWindow(
        SystemLinkContext&             Ctx,
        SubStreamID                    SubStreamID,
        DeviceMap                      DeviceID,
        uint32_t                       Width         = 0,
        uint32_t                       Height        = 0,
        int16_t                        X             = 0,
        int16_t                        Y             = 0,
        int                            ShowCmd       = SW_SHOW,
        std::string_view               Title         = "Stream Window",
        const OmniWindowEventHandlers* EventHandlers = nullptr,
        FeatureTypes                   Feature       = FeatureTypes::WindowLink
    );

    void DestroyStreamRenderWindow(SubStreamID SubStreamID);

    OmniNet::PoolConfig GetStreamWindowPoolConfig(SubStreamID SubStreamID);

  private:
    // Base function to create an async WinForge stream window.
    StreamWindow* CreateStreamWindow(
        const WindowCreationData& WindowData,
        int                       ShowCmd   = SW_SHOWNORMAL,
        HINSTANCE                 hInstance = nullptr
    );

    std::unordered_map<SubStreamID, StreamSessionEntry>     SessionRegistry;
    std::unordered_map<SubStreamID, StreamWindow*>          StreamWindowRegistry;
    std::unordered_map<SubStreamID, OmniStreamer::StreamID> StreamerIDRegistry;
};
