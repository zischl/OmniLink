#include "ScreenLink.hpp"
#include "IOLink.hpp"
#include "IOLinkContext.hpp"
#include "OmniInstances.hpp"
#include "OmniLogger.hpp"
#include "OmniRouterContext.hpp"
#include "StreamPacketDispatch.hpp"
#include "StreamSessions.hpp"
#include "system_probe_impl.hpp"

// Forwarders for ScreenLink stream render window on Client
static void ScreenWindowOnMouseInput(void* Ctx, const OmniMousePacket& Packet)
{
    auto* StreamContext = static_cast<ScreenStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->NetSession)
        return;

    StreamPackets::TransmitMousePacket(
        StreamContext->NetSession,
        StreamContext->StreamID,
        static_cast<uint16_t>(Packet.dX),
        static_cast<uint16_t>(Packet.dY),
        Packet.Buttons,
        Packet.Wheel
    );
}

static void ScreenWindowOnKeyInput(void* Ctx, const OmniKeyPacket& Packet)
{
    auto* StreamContext = static_cast<ScreenStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->NetSession)
        return;

    StreamPackets::TransmitKeyPacket(
        StreamContext->NetSession,
        StreamContext->StreamID,
        Packet.VkCode,
        Packet.ScanCode,
        Packet.Flags
    );
}

static void ScreenWindowOnResize(void* Ctx, uint32_t NewWidth, uint32_t NewHeight)
{
    auto* StreamContext = static_cast<ScreenStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->NetSession)
        return;

    StreamPackets::TransmitResizePacket(
        StreamContext->NetSession,
        StreamContext->StreamID,
        static_cast<uint16_t>(NewWidth),
        static_cast<uint16_t>(NewHeight)
    );
    StreamControlPackets::TransmitRequestIDR(StreamContext->NetSession, StreamContext->StreamID);
}

static void ScreenWindowOnDecoderReady(void* Ctx)
{
    auto* StreamContext = static_cast<ScreenStreamEntry*>(Ctx);
    if (!StreamContext || !StreamContext->NetSession)
        return;

    StreamControlPackets::TransmitReady(StreamContext->NetSession, StreamContext->StreamID);
}

static void ScreenWindowOnWindowClose(void* Ctx)
{
    auto* StreamContext = static_cast<ScreenStreamEntry*>(Ctx);
    if (StreamContext && StreamContext->ReleaseSubStream) {
        StreamContext->ReleaseSubStream(StreamContext->DeviceID, StreamContext->StreamID, true);
    }
}

OmniStreamer::StreamID SystemScreenLink::StartScreenCaptureStream(
    SystemLinkContext& Ctx, SubStreamID SubStreamID, DeviceMap DeviceID
)
{
    if (!Ctx.ActiveInstances || !Ctx.ActiveInstances->contains(DeviceID)) {
        return 0;
    }

    auto&           Instance = Ctx.ActiveInstances->at(DeviceID);
    SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
    if (!Entry || !Entry->SubStream) {
        return 0;
    }

    StreamConfig Config{};

    OmniStreamer::StreamID CaptureStreamID = Ctx.StreamSessions.AddCaptureStream(
        Ctx.GraphicsContext, Entry->SubStream, DeviceID, CaptureMode::DXGI, Config
    );
    if (CaptureStreamID == OmniStreamer::InvalidStreamID) {
        return 0;
    }

    Ctx.StreamSessions.RegisterSourceSession(
        SubStreamID, DeviceID, FeatureTypes::ScreenLink, CaptureStreamID, nullptr
    );

    Logger::log(
        "ScreenCapture started for SubStreamID={:d}, StreamID={:d}", SubStreamID, CaptureStreamID
    );
    return CaptureStreamID;
}

void SystemScreenLink::StopScreenCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID)
{
    Ctx.StreamSessions.DestroyCaptureStream(SubStreamID);
}

void SystemScreenLink::HandleScreenMouse(const OmniWinMousePacket& Packet)
{
    // Global desktop KVM forwarding for ScreenLink
    OmniMousePacket FallbackPacket{};
    FallbackPacket.dX      = Packet.NormalizedX;
    FallbackPacket.dY      = Packet.NormalizedY;
    FallbackPacket.Buttons = Packet.Buttons;
    FallbackPacket.Wheel   = Packet.Wheel;
    FallbackPacket.Flags   = OMNI_MOUSE_ABSOLUTE;
    OmniSynth::ProcMouse(FallbackPacket);
}

void SystemScreenLink::HandleScreenKey(const OmniWinKeyPacket& Packet)
{
    // Global desktop KVM forwarding for ScreenLink
    OmniKeyPacket KeyPacket{};
    KeyPacket.VkCode    = Packet.VkCode;
    KeyPacket.ScanCode  = Packet.ScanCode;
    KeyPacket.Flags     = Packet.Flags;
    KeyPacket.ExtraInfo = OMNI_INPUT_COOKIE;
    OmniSynth::ProcKey(KeyPacket);
}

OmniNet::PoolConfig SystemScreenLink::SetupClientStreamRenderWindow(
    SystemLinkContext& Ctx,
    SubStreamID        SubStreamID,
    DeviceMap          DeviceID,
    uint32_t           Width,
    uint32_t           Height
)
{
    OmniNetSession<OmniMTU>* NetSession = nullptr;
    if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
        NetSession = Ctx.ActiveInstances->at(DeviceID).InstanceSession.get();
    }

    auto Entry = std::make_unique<ScreenStreamEntry>(ScreenStreamEntry{
        .NetSession       = NetSession,
        .ReleaseSubStream = Ctx.ReleaseSubStream,
        .DeviceID         = DeviceID,
        .StreamID         = SubStreamID,
        .WindowHwnd       = NULL
    });

    ScreenStreamEntry* RawEntry = Entry.get();

    OmniWindowEventHandlers EventHandler{};
    EventHandler.Context             = RawEntry;
    EventHandler.OnMouseInput        = ScreenWindowOnMouseInput;
    EventHandler.OnKeyInput          = ScreenWindowOnKeyInput;
    EventHandler.OnResize            = ScreenWindowOnResize;
    EventHandler.OnDecoderReady      = ScreenWindowOnDecoderReady;
    EventHandler.OnWindowClose       = ScreenWindowOnWindowClose;
    EventHandler.OnWindowDragMove    = nullptr;
    EventHandler.OnWindowDragRelease = nullptr;

    OmniNet::PoolConfig PoolConfig = Ctx.StreamSessions.SetupStreamRenderWindow(
        Ctx,
        SubStreamID,
        DeviceID,
        Width,
        Height,
        0,
        0,
        SW_SHOWNORMAL,
        "Screen Link",
        &EventHandler,
        FeatureTypes::ScreenLink
    );

    StreamWindow* Window = Ctx.StreamSessions.GetStreamWindow(SubStreamID);
    if (Window) {
        RawEntry->WindowHwnd = Window->GetHwnd();
    }
    ScreenStreamEntries[SubStreamID] = std::move(Entry);
    return PoolConfig;
}

OmniNet::PoolConfig SystemScreenLink::SetState(
    SystemLinkContext& Ctx,
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            StartScreenCaptureStream(Ctx, SubStreamID, DeviceID);
        } else {
            if (SubStreamID != 0) {
                StopScreenCaptureStream(Ctx, SubStreamID);
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance   = Ctx.ActiveInstances->at(DeviceID);
                auto  SubStreams = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t StreamID : SubStreams) {
                    StopScreenCaptureStream(Ctx, StreamID);
                }
            }
        }
    } else {
        if (Action == FeatureAction::Activate) {
            uint32_t TargetW = 0, TargetH = 0;
            Ctx.Router.GetDeviceResolution(DeviceID, TargetW, TargetH);

            return SetupClientStreamRenderWindow(Ctx, SubStreamID, DeviceID, TargetW, TargetH);
        } else {
            if (SubStreamID != 0) {
                ScreenStreamEntries.erase(SubStreamID);
                Ctx.StreamSessions.DestroyStreamRenderWindow(SubStreamID);
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance = Ctx.ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t StreamID : Streams) {
                    ScreenStreamEntries.erase(StreamID);
                    Ctx.StreamSessions.DestroyStreamRenderWindow(StreamID);
                }
            }
            Logger::log(
                "ScreenLink stopped for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        }
    }
    return OmniNet::PoolConfig{};
}
