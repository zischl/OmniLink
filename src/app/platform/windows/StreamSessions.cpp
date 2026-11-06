#include "StreamSessions.hpp"
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

SystemStreamSessions::~SystemStreamSessions()
{
    for (auto& [StreamID, Window] : StreamWindowRegistry) {
        delete Window;
    }
    StreamWindowRegistry.clear();
    SessionRegistry.clear();
}

const StreamSessionEntry* SystemStreamSessions::GetSession(SubStreamID ID) const
{
    auto Iter = SessionRegistry.find(ID);
    return (Iter != SessionRegistry.end()) ? &Iter->second : nullptr;
}

StreamSessionEntry* SystemStreamSessions::GetSession(SubStreamID ID)
{
    auto Iter = SessionRegistry.find(ID);
    return (Iter != SessionRegistry.end()) ? &Iter->second : nullptr;
}

bool SystemStreamSessions::IsSourceWindow(SubStreamID ID) const
{
    const auto* Entry = GetSession(ID);
    return Entry && Entry->Role == SessionRole::Source;
}

bool SystemStreamSessions::IsStreamWindow(SubStreamID ID) const
{
    const auto* Entry = GetSession(ID);
    return Entry && Entry->Role == SessionRole::Stream;
}

void SystemStreamSessions::RegisterSourceSession(
    SubStreamID            ID,
    DeviceMap              DeviceID,
    FeatureTypes           Feature,
    OmniStreamer::StreamID StreamID,
    HWND                   SourceHwnd
)
{
    StreamSessionEntry Entry{};
    Entry.Role             = SessionRole::Source;
    Entry.Feature          = Feature;
    Entry.DeviceID         = DeviceID;
    Entry.SourceHwnd       = SourceHwnd;
    Entry.CaptureStreamID  = StreamID;
    SessionRegistry[ID]    = Entry;
    StreamerIDRegistry[ID] = StreamID;
}

void SystemStreamSessions::RegisterStreamSession(
    SubStreamID   ID,
    DeviceMap     DeviceID,
    FeatureTypes  Feature,
    StreamWindow* Window,
    void*         FeatureContext
)
{
    StreamSessionEntry Entry{};
    Entry.Role               = SessionRole::Stream;
    Entry.Feature            = Feature;
    Entry.DeviceID           = DeviceID;
    Entry.RenderWindow       = Window;
    Entry.RenderHwnd         = Window ? Window->GetHwnd() : NULL;
    Entry.FeatureContext     = FeatureContext;
    SessionRegistry[ID]      = Entry;
    StreamWindowRegistry[ID] = Window;
}

void SystemStreamSessions::UnregisterSession(SubStreamID ID)
{
    SessionRegistry.erase(ID);
    StreamWindowRegistry.erase(ID);
    StreamerIDRegistry.erase(ID);
}

StreamWindow* SystemStreamSessions::GetStreamWindow(SubStreamID ID) const
{
    auto Iter = StreamWindowRegistry.find(ID);
    return (Iter != StreamWindowRegistry.end()) ? Iter->second : nullptr;
}

OmniStreamer::StreamID SystemStreamSessions::GetStreamerID(SubStreamID ID) const
{
    auto Iter = StreamerIDRegistry.find(ID);
    return (Iter != StreamerIDRegistry.end()) ? Iter->second : 0;
}

bool SystemStreamSessions::ContainsStreamWindow(SubStreamID ID) const
{
    return StreamWindowRegistry.contains(ID);
}

void SystemStreamSessions::DestroyCaptureStream(SubStreamID ID)
{
    auto IterStreamID = StreamerIDRegistry.find(ID);
    if (IterStreamID != StreamerIDRegistry.end()) {
        Streamer.RemoveStream(IterStreamID->second);
    }
    UnregisterSession(ID);
}

OmniStreamer::StreamID SystemStreamSessions::AddCaptureStream(
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

StreamWindow* SystemStreamSessions::CreateStreamWindow(
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

OmniNet::PoolConfig SystemStreamSessions::SetupStreamRenderWindow(
    SystemLinkContext&             Ctx,
    SubStreamID                    SubStreamID,
    DeviceMap                      DeviceID,
    uint32_t                       Width,
    uint32_t                       Height,
    int16_t                        X,
    int16_t                        Y,
    int                            ShowCmd,
    std::string_view               Title,
    const OmniWindowEventHandlers* CustomHandlers,
    FeatureTypes                   Feature
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
        RegisterStreamSession(
            SubStreamID,
            DeviceID,
            Feature,
            Window,
            CustomHandlers ? CustomHandlers->Context : nullptr
        );

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

void SystemStreamSessions::DestroyStreamRenderWindow(SubStreamID SubStreamID)
{
    auto IterStreamWindows = StreamWindowRegistry.find(SubStreamID);
    if (IterStreamWindows != StreamWindowRegistry.end()) {
        delete IterStreamWindows->second;
    }
    UnregisterSession(SubStreamID);
    Logger::log("StreamWindow destroyed for SubStreamID={:d}", SubStreamID);
}

OmniNet::PoolConfig SystemStreamSessions::GetStreamWindowPoolConfig(SubStreamID SubStreamID)
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

void SystemStreamSessions::ResizeStreamWindow(SubStreamID ID, uint32_t Width, uint32_t Height)
{
    const uint32_t NewWidth  = Width;
    const uint32_t NewHeight = Height;

    if (NewWidth == 0 || NewHeight == 0) {
        return;
    }

    auto IterStreamWindows = StreamWindowRegistry.find(ID);
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

void SystemStreamSessions::HandleStreamControlPacket(
    SystemLinkContext& Ctx, const OmniStreamControlPacket& Packet, DeviceMap SenderDevice
)
{
    const SubStreamID      StreamKey  = Packet.StreamKey;
    OmniStreamer::StreamID StreamerID = GetStreamerID(StreamKey);
    if (StreamerID == 0) {
        const auto* Entry = GetSession(StreamKey);
        if (Entry && Entry->Role == SessionRole::Source) {
            StreamerID = Entry->CaptureStreamID;
        }
    }

    if (StreamerID == 0) {
        return;
    }

    switch (Packet.Command) {
    case StreamControl::Ready: {
        Streamer.SetStreamState(StreamerID, StreamState::Active);
        Logger::log("Streamer activated with IDR keyframe for SubStreamID={:d}", StreamKey);
        break;
    }

    case StreamControl::RequestIDR: {
        Logger::log(
            "IDR keyframe requested for SubStreamID={:d}, SenderDevice={:d}",
            StreamKey,
            static_cast<int>(SenderDevice)
        );
        Streamer.RequestKeyframe(StreamerID);
        break;
    }

    case StreamControl::Pause: {
        Logger::log(
            "Pause stream for SubStreamID={:d}, SenderDevice={:d}",
            StreamKey,
            static_cast<int>(SenderDevice)
        );
        Streamer.SetStreamState(StreamerID, StreamState::Paused);
        break;
    }

    case StreamControl::Resume: {
        Logger::log(
            "Resume stream for SubStreamID={:d}, SenderDevice={:d}",
            StreamKey,
            static_cast<int>(SenderDevice)
        );
        Streamer.SetStreamState(StreamerID, StreamState::Active);
        break;
    }
    }
}
