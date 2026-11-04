#include "CaptureStreams.hpp"
#include "OmniConfig.hpp"
#include "OmniInstances.hpp"
#include "OmniLogger.hpp"
#include "OmniRouterContext.hpp"
#include "SessionHandler.hpp"
#include "SubStream.hpp"
#include "SystemLink.hpp"
#include "WinForge.hpp"
#include "system_probe_impl.hpp"

#include <codecvt>
#include <dwmapi.h>

SystemCaptureStreams::~SystemCaptureStreams()
{
    for (auto& [StreamID, Window] : StreamWindowRegistry) {
        delete Window;
    }
    StreamWindowRegistry.clear();
}

StreamWindow* SystemCaptureStreams::FindStreamWindow(SubStreamID ID) const
{
    auto Iter = StreamWindowRegistry.find(ID);
    return (Iter != StreamWindowRegistry.end()) ? Iter->second : nullptr;
}

OmniStreamer::StreamID SystemCaptureStreams::FindStreamerID(SubStreamID ID) const
{
    auto Iter = StreamerIDRegistry.find(ID);
    return (Iter != StreamerIDRegistry.end()) ? Iter->second : 0;
}

bool SystemCaptureStreams::ContainsStreamWindow(SubStreamID ID) const
{
    return StreamWindowRegistry.contains(ID);
}

void SystemCaptureStreams::RegisterStreamerID(SubStreamID ID, OmniStreamer::StreamID StreamID)
{
    StreamerIDRegistry[ID] = StreamID;
}

void SystemCaptureStreams::UnregisterStreamerID(SubStreamID ID)
{
    StreamerIDRegistry.erase(ID);
}

void SystemCaptureStreams::RemoveCaptureStream(SubStreamID ID)
{
    auto IterStreamID = StreamerIDRegistry.find(ID);
    if (IterStreamID != StreamerIDRegistry.end()) {
        Streamer.RemoveStream(IterStreamID->second);
        StreamerIDRegistry.erase(IterStreamID);
    }
}

OmniStreamer::StreamID SystemCaptureStreams::AddCaptureStream(
    OmniGraphicsContext& GraphicsContext,
    OmniNetSubStream*    SubStream,
    DeviceMap            DeviceID,
    CaptureMode          Mode,
    const StreamConfig&  Config
)
{
    if (!StreamingDevice) {
        D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            featureLevels,
            _countof(featureLevels),
            D3D11_SDK_VERSION,
            &StreamingDevice,
            nullptr,
            &StreamingContext
        );
        if (SUCCEEDED(hr)) {
            ComPtr<ID3D10Multithread> multithread;
            if (SUCCEEDED(StreamingDevice->QueryInterface(IID_PPV_ARGS(&multithread)))) {
                multithread->SetMultithreadProtected(TRUE);
            }
        } else {
            Logger::log("Failed to create Streaming D3D11 Device\n");
            return 0;
        }
    }

    return Streamer.AddStream(
        StreamingDevice.Get(), StreamingContext.Get(), SubStream, DeviceID, Mode, Config
    );
}

void SystemCaptureStreams::TransmitWindowResize(
    OmniNetSession<OmniMTU>* NetSession,
    SubStreamID              WindowKey,
    DeviceMap                DeviceID,
    uint32_t                 Width,
    uint32_t                 Height
)
{
    if (!NetSession) {
        return;
    }

    OmniWinResizePacket Packet{};
    Packet.WindowKey = WindowKey;
    Packet.Width     = static_cast<uint16_t>(Width);
    Packet.Height    = static_cast<uint16_t>(Height);

    OmniNet::OmniHeader Header{};
    Header.PacketType = OmniNet::PacketType::ProcWinResize;
    Header.Target     = 0;
    Header.Flags      = 0;

    NetSession->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinResizePacket), Header);

    Logger::log(
        "WindowResizeEvent sent to WindowKey={:d}, TargetDevice={:d}, NewSize={}x{}",
        WindowKey,
        static_cast<int>(DeviceID),
        Width,
        Height
    );
}

void SystemCaptureStreams::OnStreamWindowReady(
    OmniNetSession<OmniMTU>* NetSession, SubStreamID WindowKey, DeviceMap DeviceID
)
{
    OmniStreamControlPacket Packet{};
    Packet.StreamKey = WindowKey;
    Packet.Command   = StreamControl::Ready;

    OmniNet::OmniHeader Header{};
    Header.PacketType = OmniNet::PacketType::ProcStreamControl;
    Header.Target     = 0;
    Header.Flags      = 0;

    NetSession->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinResizePacket), Header);

    Logger::log(
        "Stream Control status Ready sent to WindowKey={:d}, TargetDevice={:d}",
        WindowKey,
        static_cast<int>(DeviceID)
    );
}

void SystemCaptureStreams::RequestStreamIDR(
    OmniNetSession<OmniMTU>* NetSession, SubStreamID WindowKey, DeviceMap DeviceID
)
{
    if (!NetSession) {
        return;
    }

    OmniStreamControlPacket Packet{};
    Packet.StreamKey = WindowKey;
    Packet.Command   = StreamControl::RequestIDR;
    Packet.Param     = 0;

    OmniNet::OmniHeader Header{};
    Header.PacketType = OmniNet::PacketType::ProcStreamControl;
    Header.Target     = 0;
    Header.Flags      = 0;

    NetSession->SessionSend(
        reinterpret_cast<CHAR*>(&Packet), sizeof(OmniStreamControlPacket), Header
    );
}

StreamWindow* SystemCaptureStreams::CreateStreamWindow(
    const WindowCreationData& WindowData, int ShowCmd, HINSTANCE hInstance
)
{
    WinForge* Window = new WinForge();

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    std::wstring WindowTitle =
        converter.from_bytes(reinterpret_cast<const char*>(WindowData.GetTitleU8().data()));

    if (!hInstance) {
        hInstance = GetModuleHandle(nullptr);
    }

    Window->CreateWindowAsync(
        WindowTitle.c_str(), hInstance, ShowCmd, WindowData.Width, WindowData.Height
    );
    return Window;
}

void SystemCaptureStreams::OnStreamWindowClose(
    std::function<void(DeviceMap, SubStreamID, bool)> ReleaseSubStream,
    SubStreamID                                       WindowKey,
    DeviceMap                                         DeviceID
)
{
    std::thread([this, ReleaseSubStream, WindowKey, DeviceID]() {
        if (ReleaseSubStream) {
            ReleaseSubStream(DeviceID, WindowKey, true);
        }
        DestroyStreamRenderWindow(WindowKey);
    }).detach();
}

OmniNet::PoolConfig SystemCaptureStreams::SetupStreamRenderWindow(
    SystemLinkContext&             Ctx,
    SubStreamID                    SubStreamID,
    DeviceMap                      DeviceID,
    uint32_t                       Width,
    uint32_t                       Height,
    int16_t                        X,
    int16_t                        Y,
    int                            ShowCmd,
    std::string_view               Title,
    const OmniWindowEventHandlers* CustomHandlers
)
{
    WindowCreationData WGC{Title};

    if (Width == 0 || Height == 0) {
        Device::MonitorRes LocalRes = Device::GetMonitorResolution();

        WGC.Width  = (Width > 0) ? Width : LocalRes.Width;
        WGC.Height = (Height > 0) ? Height : LocalRes.Height;
    } else {
        WGC.Width  = Width;
        WGC.Height = Height;
    }

    StreamWindow* Window = CreateStreamWindow(WGC, ShowCmd);
    Logger::log(
        "StreamWindow created for device {:d}, SubStreamID={:d}, Initial Res: {}x{}",
        static_cast<int>(DeviceID),
        SubStreamID,
        Width,
        Height
    );

    OmniNet::PoolConfig Config{};
    if (Window) {
        StreamWindowRegistry[SubStreamID] = Window;

        HWND WindowHwnd = Window->GetHwnd();
        if (X != 0 || Y != 0) {
            if (WindowHwnd) {
                SetWindowPos(
                    WindowHwnd, NULL, X, Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
                );
            }
        }

        if (CustomHandlers) {
            Window->SetEventForwarder(*CustomHandlers);
            Window->SetEventForwarding(true);
        }

        Window->GetFramePool(
            Config.Data, Config.DataSize, Config.NumSlots, &Config.OnSlotComplete, Config.Ctx
        );
    }
    return Config;
}

void SystemCaptureStreams::DestroyStreamRenderWindow(SubStreamID SubStreamID)
{
    auto IterStreamWindows = StreamWindowRegistry.find(SubStreamID);
    if (IterStreamWindows != StreamWindowRegistry.end()) {
        delete IterStreamWindows->second;
        StreamWindowRegistry.erase(IterStreamWindows);
    }
    Logger::log("StreamWindow destroyed for SubStreamID={:d}", SubStreamID);
}

OmniNet::PoolConfig SystemCaptureStreams::GetStreamWindowPoolConfig(SubStreamID SubStreamID)
{
    auto IterStreamWindows = StreamWindowRegistry.find(SubStreamID);
    if (IterStreamWindows != StreamWindowRegistry.end() && IterStreamWindows->second) {
        OmniNet::PoolConfig PoolConfig{};
        IterStreamWindows->second->GetFramePool(
            PoolConfig.Data,
            PoolConfig.DataSize,
            PoolConfig.NumSlots,
            &PoolConfig.OnSlotComplete,
            PoolConfig.Ctx
        );
        return PoolConfig;
    }
    return OmniNet::PoolConfig{};
}

void SystemCaptureStreams::HandleStreamWindowResize(const OmniWinResizePacket Packet)
{
    const SubStreamID StreamID  = Packet.WindowKey;
    const uint32_t    NewWidth  = Packet.Width;
    const uint32_t    NewHeight = Packet.Height;

    if (NewWidth == 0 || NewHeight == 0) {
        return;
    }

    auto IterStreamWindows = StreamWindowRegistry.find(StreamID);
    if (IterStreamWindows != StreamWindowRegistry.end() && IterStreamWindows->second) {
        StreamWindow* Window     = IterStreamWindows->second;
        HWND          StreamHwnd = Window->GetHwnd();

        if (StreamHwnd && IsWindow(StreamHwnd)) {
            RECT CurrentRect{};
            if (GetWindowRect(StreamHwnd, &CurrentRect)) {
                uint32_t CurrentWidth = static_cast<uint32_t>(CurrentRect.right - CurrentRect.left);
                uint32_t CurrentHeight =
                    static_cast<uint32_t>(CurrentRect.bottom - CurrentRect.top);
                if (CurrentWidth == NewWidth && CurrentHeight == NewHeight) {
                    return;
                }
            }
            Window->UpdateDimensions(NewWidth, NewHeight);
            SetWindowPos(
                StreamHwnd,
                nullptr,
                0,
                0,
                static_cast<int>(NewWidth),
                static_cast<int>(NewHeight),
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
    }
}

void SystemCaptureStreams::HandleStreamWindowMouse(const OmniWinMousePacket& Packet)
{
    // Fallback for capture streams without mapped HWND (e.g. ScreenLink)
    OmniMousePacket FallbackPacket{};
    FallbackPacket.dX      = Packet.NormalizedX;
    FallbackPacket.dY      = Packet.NormalizedY;
    FallbackPacket.Buttons = Packet.Buttons;
    FallbackPacket.Wheel   = Packet.Wheel;
    FallbackPacket.Flags   = OMNI_MOUSE_ABSOLUTE;
    OmniSynth::ProcMouse(FallbackPacket);
}

void SystemCaptureStreams::HandleStreamWindowKey(const OmniWinKeyPacket& Packet)
{
    OmniKeyPacket KeyPacket{};
    KeyPacket.VkCode    = Packet.VkCode;
    KeyPacket.ScanCode  = Packet.ScanCode;
    KeyPacket.Flags     = Packet.Flags;
    KeyPacket.ExtraInfo = OMNI_INPUT_COOKIE;
    OmniSynth::ProcKey(KeyPacket);
}

void SystemCaptureStreams::HandleStreamControlPacket(
    SystemLinkContext& Ctx, const OmniStreamControlPacket& Packet, DeviceMap SenderDevice
)
{
    const SubStreamID StreamKey = Packet.StreamKey;

    switch (Packet.Command) {
    case StreamControl::Ready: {
        auto IterStreamID = StreamerIDRegistry.find(StreamKey);
        if (IterStreamID != StreamerIDRegistry.end()) {
            Streamer.SetStreamState(IterStreamID->second, StreamState::Active);
            Logger::log("Streamer activated wwith IDR keyframe for SubStreamID={:d}", StreamKey);
        }
        break;
    }

    case StreamControl::RequestIDR: {
        Logger::log(
            "IDR keyframe requested for SubStreamID={:d}, SenderDevice={:d}",
            StreamKey,
            static_cast<int>(SenderDevice)
        );
        auto IterStreamID = StreamerIDRegistry.find(StreamKey);
        if (IterStreamID != StreamerIDRegistry.end()) {
            Streamer.RequestKeyframe(IterStreamID->second);
        }
        break;
    }

    case StreamControl::Pause: {
        Logger::log(
            "Pause stream for SubStreamID={:d}, SenderDevice={:d}",
            StreamKey,
            static_cast<int>(SenderDevice)
        );
        auto IterStreamID = StreamerIDRegistry.find(StreamKey);
        if (IterStreamID != StreamerIDRegistry.end()) {
            Streamer.SetStreamState(IterStreamID->second, StreamState::Paused);
        }
        break;
    }

    case StreamControl::Resume: {
        Logger::log(
            "Resume stream for SubStreamID={:d}, SenderDevice={:d}",
            StreamKey,
            static_cast<int>(SenderDevice)
        );
        auto IterStreamID = StreamerIDRegistry.find(StreamKey);
        if (IterStreamID != StreamerIDRegistry.end()) {
            Streamer.SetStreamState(IterStreamID->second, StreamState::Active);
        }
        break;
    }
    }
}
