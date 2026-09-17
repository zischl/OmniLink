#include "SystemLink.h"
#include "ClipboardTypes.hpp"
#include "OmniConfig.hpp"
#include "OmniEnums.hpp"
#include "OmniTCPStream.h"
#include "SessionHandler.hpp"
#include "SessionTypes.hpp"
#include "WinForge.hpp"
#include "system_probe_impl.hpp"

#include <atomic>
#include <codecvt>
#include <d3d11.h>

OmniSystemLink::OmniSystemLink(OmniGraphicsContext& GraphicsContext)
    : GraphicsContext(GraphicsContext)
{
}

OmniSystemLink::~OmniSystemLink()
{
    for (auto& [StreamID, Window] : StreamWindowRegistry) {
        delete Window;
    }
    StreamWindowRegistry.clear();
}

void OmniSystemLink::SetupSystemLink(HINSTANCE hInstance_, int nCmdShow_, HWND WindowID_)
{
    hInstance = hInstance_;
    nCmdShow  = nCmdShow_;
    WindowID  = WindowID_;
}

OmniStreamer::StreamID OmniSystemLink::AddCaptureStream(
    OmniNetSubStream* SubStream, DeviceMap DeviceID, CaptureMode Mode, const StreamConfig& Config
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

void OmniSystemLink::TransmitWindowResizeEvent(
    SubStreamID WindowKey, DeviceMap DeviceID, uint32_t Width, uint32_t Height
)
{
    if (!ActiveInstances || !ActiveInstances->contains(DeviceID)) {
        return;
    }

    OmniActiveInstance& Instance = ActiveInstances->at(DeviceID);
    if (!Instance.InstanceSession) {
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

    Instance.InstanceSession->SessionSend(
        reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinResizePacket), Header
    );

    Logger::log(
        "WindowResizeEvent sent to WindowKey={:d}, TargetDevice={:d}, NewSize={}x{}",
        WindowKey,
        static_cast<int>(DeviceID),
        Width,
        Height
    );
}

OmniStreamer::StreamID
OmniSystemLink::StartWindowCaptureStream(SubStreamID SubStreamID, DeviceMap DeviceID, HWND Hwnd)
{
    if (!Hwnd || !IsWindow(Hwnd) || !ActiveInstances || !ActiveInstances->contains(DeviceID))
        return 0;

    auto&           Instance = ActiveInstances->at(DeviceID);
    SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
    if (!Entry || !Entry->SubStream)
        return 0;

    StreamConfig Config{};
    Config.WindowHandle = Hwnd;
    RECT rect           = {};
    GetWindowRect(Hwnd, &rect);
    Config.Width    = static_cast<uint32_t>(rect.right - rect.left);
    Config.Height   = static_cast<uint32_t>(rect.bottom - rect.top);
    Config.OnResize = [this, DeviceID, SubStreamID](uint32_t Width, uint32_t Height) {
        TransmitWindowResizeEvent(SubStreamID, DeviceID, Width, Height);
    };

    OmniStreamer::StreamID StreamID =
        AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::WGC_Window, Config);
    StreamerIDRegistry[SubStreamID] = StreamID;

    Logger::log(
        "WindowCapture started for SubStreamID={:d}, HWND={:p}, StreamID={:d}",
        SubStreamID,
        reinterpret_cast<void*>(Hwnd),
        StreamID
    );
    return StreamID;
}

void OmniSystemLink::StopWindowCaptureStream(SubStreamID SubStreamID)
{
    auto IterStreamID = StreamerIDRegistry.find(SubStreamID);
    if (IterStreamID != StreamerIDRegistry.end()) {
        Streamer.RemoveStream(IterStreamID->second);
        StreamerIDRegistry.erase(IterStreamID);
    }

    auto IterHwnd = SubStream2HwndRegistry.find(SubStreamID);
    if (IterHwnd != SubStream2HwndRegistry.end()) {
        Hwnd2SubStreamRegistry.erase(IterHwnd->second);
        SubStream2HwndRegistry.erase(IterHwnd);
    }
    SubStreamToDevice.erase(SubStreamID);

    Logger::log("WindowCapture stopped for SubStreamID={:d}", SubStreamID);
}

StreamWindow* OmniSystemLink::CreateStreamWindow(const WindowCreationData& WindowData, int ShowCmd)
{
    WinForge* Window = new WinForge();

    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

    std::wstring WindowTitle =
        converter.from_bytes(reinterpret_cast<const char*>(WindowData.GetTitleU8().data()));

    Window->CreateWindowAsync(
        WindowTitle.c_str(), hInstance, ShowCmd, WindowData.Width, WindowData.Height
    );
    return Window;
}

void OmniSystemLink::OnStreamWindowClose(SubStreamID WindowKey, DeviceMap DeviceID)
{
    Logger::log(
        "Stream render window closed for SubStreamID={:d}, DeviceID={:d}",
        WindowKey,
        static_cast<int>(DeviceID)
    );

    std::thread([this, DeviceID, WindowKey]() {
        if (ReleaseSubStream) {
            ReleaseSubStream(DeviceID, WindowKey, true);
        } else {
            DestroyStreamRenderWindow(WindowKey);
        }
    }).detach();
}

OmniNet::PoolConfig OmniSystemLink::SetupStreamRenderWindow(
    SubStreamID SubStreamID,
    DeviceMap   DeviceID,
    uint32_t    Width,
    uint32_t    Height,
    int16_t     X,
    int16_t     Y,
    int         ShowCmd
)
{
    WindowCreationData WGC{"Window Stream Window"};

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

        if (X != 0 || Y != 0) {
            HWND h = Window->GetHwnd();
            if (h) {
                SetWindowPos(h, NULL, X, Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }

        OmniNetSession<OmniMTU>* NetSession = nullptr;
        if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
            NetSession = ActiveInstances->at(DeviceID).InstanceSession.get();
        }

        auto& StreamerContext = StreamContexts[SubStreamID] = WindowStreamContext{
            .SysLink = this, .Session = NetSession, .DeviceID = DeviceID, .WindowKey = SubStreamID
        };

        OmniWindowEventHandlers EventHandler{};
        EventHandler.Context = &StreamerContext;

        EventHandler.OnMouseInput = [](void* Ctx, const OmniMousePacket& Packet) {
            auto* StreamContext = static_cast<WindowStreamContext*>(Ctx);
            if (StreamContext && StreamContext->Session) {
                OmniNet::OmniHeader Header;
                Header.Target     = 0;
                Header.PacketType = OmniNet::PacketType::ProcMouse;
                Header.Flags      = 0;
                StreamContext->Session->SessionSend(
                    reinterpret_cast<CHAR*>(const_cast<OmniMousePacket*>(&Packet)),
                    sizeof(OmniMousePacket),
                    Header
                );
            }
        };

        EventHandler.OnKeyInput = [](void* Ctx, const OmniKeyPacket& Packet) {
            auto* StreamContext = static_cast<WindowStreamContext*>(Ctx);
            if (StreamContext && StreamContext->Session) {
                OmniNet::OmniHeader Header;
                Header.Target     = 0;
                Header.PacketType = OmniNet::PacketType::ProcKey;
                Header.Flags      = 0;
                StreamContext->Session->SessionSend(
                    reinterpret_cast<CHAR*>(const_cast<OmniKeyPacket*>(&Packet)),
                    sizeof(OmniKeyPacket),
                    Header
                );
            }
        };

        EventHandler.OnResize = [](void* Ctx, uint32_t NewWidth, uint32_t NewHeight) {
            auto* StreamContext = static_cast<WindowStreamContext*>(Ctx);
            if (StreamContext && StreamContext->SysLink) {
                StreamContext->SysLink->TransmitWindowResizeEvent(
                    StreamContext->WindowKey, StreamContext->DeviceID, NewWidth, NewHeight
                );
            }
        };

        EventHandler.OnWindowClose = [](void* Ctx) {
            auto* StreamContext = static_cast<WindowStreamContext*>(Ctx);
            if (StreamContext && StreamContext->SysLink) {
                StreamContext->SysLink->OnStreamWindowClose(
                    StreamContext->WindowKey, StreamContext->DeviceID
                );
            }
        };

        Window->SetEventForwarder(EventHandler);
        Window->SetEventForwarding(true);

        Window->GetFramePool(
            Config.Data, Config.DataSize, Config.NumSlots, &Config.OnSlotComplete, Config.Ctx
        );
    }
    return Config;
}

void OmniSystemLink::DestroyStreamRenderWindow(SubStreamID SubStreamID)
{
    auto IterStreamWindows = StreamWindowRegistry.find(SubStreamID);
    if (IterStreamWindows != StreamWindowRegistry.end()) {
        delete IterStreamWindows->second;
        StreamWindowRegistry.erase(IterStreamWindows);
    }
    StreamContexts.erase(SubStreamID);
    Logger::log("StreamWindow destroyed for SubStreamID={:d}", SubStreamID);
}

void OmniSystemLink::BindIOLinkSession(DeviceMap DeviceID)
{
    if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
        auto& instance = ActiveInstances->at(DeviceID);
        OmniRouter.RegisterSession(DeviceID, instance.InstanceSession.get());
    }
}

void OmniSystemLink::UnbindIOLinkSession(DeviceMap DeviceID)
{
    OmniRouter.UnregisterSession(DeviceID);
    if (InputLinkCtx.ActiveEdge == DeviceID) {
        InputLinkCtx.DeactivateEdge();
    }
    InputLink.ConditionManager.Remove(DeviceID);

    if (InputLink.ConditionManager.Empty() && InputLink.GetEdgeProbeState()) {
        InputLink.ToggleEdgeProbe(WindowID);
    }

    SyncInputFilter();
}

OmniNet::PoolConfig OmniSystemLink::SetScreenLinkState(
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    (void)Context;
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            if (ActiveInstances && ActiveInstances->contains(DeviceID) && SubStreamID != 0) {
                auto&           Instance = ActiveInstances->at(DeviceID);
                SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
                if (Entry && Entry->SubStream) {
                    StreamConfig           Config{};
                    OmniStreamer::StreamID StreamID =
                        AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::DXGI, Config);
                    StreamerIDRegistry[SubStreamID] = StreamID;
                }
            }
            Logger::log(
                "CaptureStream on ScreenLink started for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        } else {
            if (SubStreamID != 0) {
                auto it = StreamerIDRegistry.find(SubStreamID);
                if (it != StreamerIDRegistry.end()) {
                    Streamer.RemoveStream(it->second);
                    StreamerIDRegistry.erase(it);
                }
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t id : Streams) {
                    auto it = StreamerIDRegistry.find(id);
                    if (it != StreamerIDRegistry.end()) {
                        Streamer.RemoveStream(it->second);
                        StreamerIDRegistry.erase(it);
                    }
                }
            }
            Logger::log(
                "ScreenLink stopped for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        }
    } else {
        if (Action == FeatureAction::Activate) {
            uint32_t TargetW = 0, TargetH = 0;
            OmniRouter.GetDeviceResolution(DeviceID, TargetW, TargetH);

            WindowCreationData WindowConfig{"Screen Link"};
            WindowConfig.Width   = TargetW;
            WindowConfig.Height  = TargetH;
            StreamWindow* Window = CreateStreamWindow(WindowConfig, SW_SHOWNORMAL);
            Logger::log(
                "StreamWindow created for device {:d}, SubStreamID={:d}, Res: {}x{}",
                static_cast<int>(DeviceID),
                SubStreamID,
                TargetW,
                TargetH
            );

            OmniNet::PoolConfig Config{};
            if (Window) {
                StreamWindowRegistry[SubStreamID] = Window;

                OmniNetSession<OmniMTU>* NetSession = nullptr;
                if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                    NetSession = ActiveInstances->at(DeviceID).InstanceSession.get();
                }

                auto& StreamerContext = StreamContexts[SubStreamID] = WindowStreamContext{
                    .SysLink   = this,
                    .Session   = NetSession,
                    .DeviceID  = DeviceID,
                    .WindowKey = SubStreamID
                };

                OmniWindowEventHandlers WindowEvent{};
                WindowEvent.Context = &StreamerContext;

                WindowEvent.OnMouseInput = [](void* Ctx, const OmniMousePacket& Packet) {
                    auto* StreamerContext = static_cast<WindowStreamContext*>(Ctx);
                    if (StreamerContext && StreamerContext->Session) {
                        OmniNet::OmniHeader Header;
                        Header.Target     = 0;
                        Header.PacketType = OmniNet::PacketType::ProcMouse;
                        Header.Flags      = 0;
                        StreamerContext->Session->SessionSend(
                            reinterpret_cast<CHAR*>(const_cast<OmniMousePacket*>(&Packet)),
                            sizeof(OmniMousePacket),
                            Header
                        );
                    }
                };

                WindowEvent.OnKeyInput = [](void* Ctx, const OmniKeyPacket& Packet) {
                    auto* StreamerContext = static_cast<WindowStreamContext*>(Ctx);
                    if (StreamerContext && StreamerContext->Session) {
                        OmniNet::OmniHeader Header;
                        Header.Target     = 0;
                        Header.PacketType = OmniNet::PacketType::ProcKey;
                        Header.Flags      = 0;
                        StreamerContext->Session->SessionSend(
                            reinterpret_cast<CHAR*>(const_cast<OmniKeyPacket*>(&Packet)),
                            sizeof(OmniKeyPacket),
                            Header
                        );
                    }
                };

                Window->SetEventForwarder(WindowEvent);
                Window->SetEventForwarding(true);

                Window->GetFramePool(
                    Config.Data,
                    Config.DataSize,
                    Config.NumSlots,
                    &Config.OnSlotComplete,
                    Config.Ctx
                );
            }
            return Config;
        } else {
            if (SubStreamID != 0) {
                auto IterStreamWindows = StreamWindowRegistry.find(SubStreamID);
                if (IterStreamWindows != StreamWindowRegistry.end()) {
                    delete IterStreamWindows->second;
                    StreamWindowRegistry.erase(IterStreamWindows);
                }
                StreamContexts.erase(SubStreamID);
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t StreamID : Streams) {
                    auto IterStreamWindows = StreamWindowRegistry.find(StreamID);
                    if (IterStreamWindows != StreamWindowRegistry.end()) {
                        delete IterStreamWindows->second;
                        StreamWindowRegistry.erase(IterStreamWindows);
                    }
                    StreamContexts.erase(StreamID);
                }
                auto IterStreamWindows = StreamWindowRegistry.find(0);
                if (IterStreamWindows != StreamWindowRegistry.end()) {
                    delete IterStreamWindows->second;
                    StreamWindowRegistry.erase(IterStreamWindows);
                }
                StreamContexts.erase(0);
            }
            Logger::log(
                "StreamWindow closed for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        }
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetWindowLinkState(
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            if (SubStreamID == 0) {
                if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                    auto& Instance = ActiveInstances->at(DeviceID);
                    OmniRouter.RegisterWindowSession(DeviceID, Instance.InstanceSession.get());
                }

                if (OmniRouter.GetWindowSessionCount() == 1) {
                    DragLink.WindowDragCallback =
                        [this](HWND Hwnd, DeviceMap TargetDevice, WinDragAction WinAction) {
                            return HandleWindowDragEvent(Hwnd, TargetDevice, WinAction);
                        };
                    DragLink.WindowMoveListener(true);
                }

                Logger::log("WindowLink enabled for DeviceID {:d}", static_cast<int>(DeviceID));
            } else {
                StartWindowCaptureStream(SubStreamID, DeviceID, reinterpret_cast<HWND>(Context));
            }
        } else {
            if (SubStreamID == 0) {
                OmniRouter.UnregisterWindowSession(DeviceID);

                if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                    auto& Instance = ActiveInstances->at(DeviceID);
                    auto  Streams  = Instance.GetSubStreams(FeatureTypes::WindowLink);
                    for (uint16_t StreamID : Streams) {
                        StopWindowCaptureStream(StreamID);
                    }
                }

                if (OmniRouter.GetWindowSessionCount() == 0) {
                    DragLink.WindowMoveListener(false);
                    DragLink.WindowDragCallback = nullptr;
                }
                Logger::log("WindowLink disabled for DeviceID {:d}", static_cast<int>(DeviceID));
            } else {
                StopWindowCaptureStream(SubStreamID);
            }
        }
    } else {
        if (Action == FeatureAction::Activate) {
            Logger::log(
                "WindowLink capability activated (Inbound) for device {:d}",
                static_cast<int>(DeviceID)
            );
        } else {
            if (SubStreamID != 0) {
                DestroyStreamRenderWindow(SubStreamID);
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::WindowLink);
                for (uint16_t StreamID : Streams) {
                    DestroyStreamRenderWindow(StreamID);
                }
            }
        }
    }
    return OmniNet::PoolConfig{};
}

// Register/Unregister the edge trigger condition for this device and bind/Unbind the net session.
// Setup Edge Probe and Input Shields if not active.. or... remove.
OmniNet::PoolConfig OmniSystemLink::SetInputLinkState(
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    (void)SubStreamID;
    (void)Context;
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            InputLink.AddEdgeCondition(DeviceID);
            BindIOLinkSession(DeviceID);

            if (!InputLink.GetEdgeProbeState())
                InputLink.ToggleEdgeProbe(WindowID);

            SyncInputFilter();

            Logger::log("InputLink enabled for DeviceID {:d}", static_cast<int>(DeviceID));
        } else {
            UnbindIOLinkSession(DeviceID);
            Logger::log("InputLink disabled for DeviceID {:d}", static_cast<int>(DeviceID));
        }
    } else {
        Logger::log(
            "InputLink {:s} (Inbound) for DeviceID {:d}",
            Action == FeatureAction::Activate ? "enabled" : "disabled",
            static_cast<int>(DeviceID)
        );
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetAudioLinkState(
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    (void)Context;
    if (Route == FeatureActionRoute::Outbound) {
        Logger::log(
            "{:s} AudioLink for DeviceID {:d}, SubStreamID={:d}",
            Action == FeatureAction::Activate ? "Starting" : "Stopping",
            static_cast<int>(DeviceID),
            SubStreamID
        );

        if (Action == FeatureAction::Activate) {
            OmniNetSubStream* TargetSubStream = nullptr;
            if (ActiveInstances && ActiveInstances->contains(DeviceID) && SubStreamID != 0) {
                auto&           Instance = ActiveInstances->at(DeviceID);
                SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
                if (Entry && Entry->SubStream) {
                    TargetSubStream = Entry->SubStream;
                }
            }

            if (TargetSubStream && DeviceID < DeviceMap::END) {
                if (!AudioLink.GetCaptureThreadState()) {
                    if (AudioLink.Init(AudioCaptureMode::DesktopOnly)) {
                        AudioLink.SetPacketCallback(
                            [this](
                                const uint8_t* Data, size_t Size, const AudioFrameHeader& Header
                            ) {
                                (void)Header;
                                for (auto& SubStreamSlot : ActiveAudioStreams) {
                                    auto* SubStream = SubStreamSlot.load(std::memory_order_acquire);
                                    if (SubStream) {
                                        SubStream->ChunkedSend(
                                            reinterpret_cast<CHAR*>(const_cast<uint8_t*>(Data)),
                                            static_cast<int>(Size)
                                        );
                                    }
                                }
                            }
                        );
                        AudioLink.Start();
                    }
                }

                auto* PrevStream = ActiveAudioStreams[DeviceID].exchange(
                    TargetSubStream, std::memory_order_acq_rel
                );
                if (!PrevStream) {
                    AudioStreamCount.fetch_add(1, std::memory_order_acq_rel);
                }
            }
        } else {
            if (DeviceID < DeviceMap::END) {
                if (ActiveAudioStreams[DeviceID].exchange(nullptr, std::memory_order_acq_rel) !=
                    nullptr) {
                    if (AudioStreamCount.fetch_sub(1, std::memory_order_acq_rel) <= 1) {
                        AudioLink.Stop();
                    }
                }
            }
        }
    } else {
        Logger::log(
            "{:s} AudioLink for DeviceID {:d}, SubStreamID={:d}",
            Action == FeatureAction::Activate ? "Starting" : "Stopping",
            static_cast<int>(DeviceID),
            SubStreamID
        );

        if (Action == FeatureAction::Activate) {
            auto Renderer = std::make_unique<AudioRender>();
            if (Renderer->Init()) {
                Renderer->Start();
                OmniNet::PoolConfig Config{};
                Renderer->GetBufferPool(
                    Config.Data,
                    Config.DataSize,
                    Config.NumSlots,
                    &Config.OnSlotComplete,
                    Config.Ctx
                );
                if (SubStreamID != 0) {
                    AudioRenderers[SubStreamID] = std::move(Renderer);
                }
                return Config;
            }
        } else {
            if (SubStreamID != 0) {
                auto Iter = AudioRenderers.find(SubStreamID);
                if (Iter != AudioRenderers.end()) {
                    if (Iter->second) {
                        Iter->second->Stop();
                    }
                    AudioRenderers.erase(Iter);
                }
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::AudioLink);
                for (auto StreamID : Streams) {
                    auto Iter = AudioRenderers.find(StreamID);
                    if (Iter != AudioRenderers.end()) {
                        if (Iter->second) {
                            Iter->second->Stop();
                        }
                        AudioRenderers.erase(Iter);
                    }
                }
            }
        }
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetClipboardLinkState(
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    (void)SubStreamID;
    (void)Route;
    (void)Context;

    uint16_t DeviceBit = (DeviceID < DeviceMap::END)
                             ? static_cast<uint16_t>(1U << static_cast<uint8_t>(DeviceID))
                             : 0;

    if (Action == FeatureAction::Activate) {
        uint16_t PrevMask =
            ActiveClipboardSubscriptions.fetch_or(DeviceBit, std::memory_order_acq_rel);
        if (PrevMask == 0 && !ClipboardLink.GetState()) {
            ClipboardLink.StartMonitoring(
                WindowID,
                [this](const std::string& Text) { TransmitClipboard(Text); },
                [this](const ClipboardManifest& Manifest) { TransmitClipboardManifest(Manifest); },
                [this](const ClipboardManifest& Manifest, UINT Format) {
                    return ReceiveClipboardData(Manifest, Format);
                }
            );
        }
    } else if (Action == FeatureAction::Deactivate) {
        uint16_t PrevMask =
            ActiveClipboardSubscriptions.fetch_and(~DeviceBit, std::memory_order_acq_rel);
        if ((PrevMask & DeviceBit) != 0 && (PrevMask & ~DeviceBit) == 0) {
            ClipboardLink.StopMonitoring();
            std::lock_guard<std::mutex> Lock(ClipboardStreamsMutex);
            for (auto& [StreamID, Stream] : ActiveClipboardStreams) {
                if (Stream) {
                    Stream->End();
                }
            }
            ActiveClipboardStreams.clear();
        }
    }

    Logger::log(
        "{:s} ClipboardSync for DeviceID {:d}",
        Action == FeatureAction::Activate ? "Enabled" : "Disabled",
        static_cast<int>(DeviceID)
    );
    return OmniNet::PoolConfig{};
}

void OmniSystemLink::TransmitClipboard(const std::string& Text)
{
    if (Text.empty() || !ActiveInstances)
        return;

    std::vector<uint8_t> Payload(1 + Text.size());
    Payload[0] = static_cast<uint8_t>(ClipboardOp::LightGram);
    std::memcpy(Payload.data() + 1, Text.data(), Text.size());

    OmniNet::OmniHeader Header;
    Header.PacketType = OmniNet::PacketType::ProcClipboard;
    Header.Target     = 0;
    Header.Flags      = 0;

    for (auto& [DevID, Instance] : *ActiveInstances) {
        if (Instance.GetFeatureState(FeatureTypes::ClipboardLink, FeatureActionRoute::Outbound)) {
            if (Instance.InstanceSession) {
                Instance.InstanceSession->SessionSend(
                    reinterpret_cast<char*>(Payload.data()),
                    static_cast<int>(Payload.size()),
                    Header
                );
                Logger::log(
                    "Transmit completed for {:d} bytes of clipboard data to DeviceID {:d}",
                    Text.size(),
                    static_cast<int>(DevID)
                );
            }
        }
    }
}

void OmniSystemLink::TransmitClipboardManifest(const ClipboardManifest& Manifest)
{
    if (!ActiveInstances)
        return;

    static std::atomic<uint32_t> GlobalStreamID{1};
    uint32_t                     StreamID = GlobalStreamID.fetch_add(1);

    auto Stream = std::make_shared<OmniTCPStream>(StreamID);
    if (!Stream->StartServer(0)) {
        Logger::log("Failed to start TCP stream server for clipboard link");
        return;
    }

    ClipboardManifest CManifest = Manifest;
    CManifest.StreamID          = StreamID;
    CManifest.ServerPort        = Stream->GetLocalPort();

    for (auto& [DevID, Instance] : *ActiveInstances) {
        Instance.RegisterTCPStream(StreamID, Stream);
    }

    std::vector<uint8_t>      LocalBuffer;
    std::vector<std::wstring> LocalFilePaths;

    int Retries = 5;
    while (!OpenClipboard(nullptr) && Retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (OpenClipboard(nullptr)) {
        if (CManifest.Category == ClipboardCategory::Image ||
            CManifest.Category == ClipboardCategory::Text) {
            HANDLE HData = GetClipboardData(CManifest.WinFormatID);
            if (HData) {
                size_t Size = GlobalSize(HData);
                void*  Ptr  = GlobalLock(HData);
                if (Ptr) {
                    if (Size > 0) {
                        LocalBuffer.resize(Size);
                        std::memcpy(LocalBuffer.data(), Ptr, Size);
                    }
                    GlobalUnlock(HData);
                }
            }
        } else if (CManifest.Category == ClipboardCategory::FileList) {
            HANDLE DropHandle = GetClipboardData(CF_HDROP);
            if (DropHandle) {
                HDROP Drop = static_cast<HDROP>(GlobalLock(DropHandle));
                if (Drop) {
                    UINT FileCount = DragQueryFileW(Drop, 0xFFFFFFFF, nullptr, 0);
                    for (UINT i = 0; i < FileCount; ++i) {
                        wchar_t FilePath[MAX_PATH]{};
                        if (DragQueryFileW(Drop, i, FilePath, MAX_PATH) > 0) {
                            LocalFilePaths.push_back(FilePath);
                        }
                    }
                    GlobalUnlock(DropHandle);
                }
            }
        }
        CloseClipboard();
    }

    auto StreamProgressData = std::make_shared<StreamProgress>();
    StreamProgressData->TotalBytes.store(CManifest.TotalSizeBytes, std::memory_order_relaxed);
    StreamProgressData->BytesTransferred.store(0, std::memory_order_relaxed);
    StreamProgressData->StreamState.store(true, std::memory_order_relaxed);
    StreamProgressData->Cancel.store(false, std::memory_order_relaxed);

    if (ClipboardCtx && ClipboardCtx->OnStreamEvent &&
        CManifest.TotalSizeBytes > LIGHTGRAM_MAX_SIZE) {
        std::string DisplayName =
            CManifest.Items.empty() ? "Clipboard Item" : CManifest.Items[0].ItemName;

        ClipboardStreamEvent Ev(
            CManifest.StreamID,
            DeviceMap::END,
            DisplayName,
            CManifest.Category == ClipboardCategory::Image
                ? "Image"
                : (CManifest.Category == ClipboardCategory::FileList ? "File" : "Text"),
            CManifest.TotalSizeBytes,
            true,
            StreamProgressData
        );
        ClipboardCtx->OnStreamEvent(Ev);
    }

    std::thread([Stream,
                 Buffer    = std::move(LocalBuffer),
                 FilePaths = std::move(LocalFilePaths),
                 Progress  = StreamProgressData]() {
        if (Stream->AcceptClient(15000)) {
            if (!Buffer.empty()) {
                Stream->StreamBuffer(Buffer.data(), Buffer.size());
                if (Progress) {
                    Progress->BytesTransferred.store(Buffer.size(), std::memory_order_relaxed);
                }
            } else if (!FilePaths.empty()) {
                for (const auto& FilePath : FilePaths) {
                    if (Progress && Progress->Cancel.load(std::memory_order_relaxed)) {
                        break;
                    }
                    Stream->StreamFile(FilePath);
                }
            }
        }
        if (Progress) {
            Progress->StreamState.store(false, std::memory_order_relaxed);
        }
        Stream->End();
    }).detach();

    std::vector<uint8_t> Serialized = ClipboardManifest::Serialize(CManifest);
    std::vector<uint8_t> Payload(1 + Serialized.size());
    Payload[0] = static_cast<uint8_t>(ClipboardOp::Manifest);
    std::memcpy(Payload.data() + 1, Serialized.data(), Serialized.size());

    OmniNet::OmniHeader Header;
    Header.PacketType = OmniNet::PacketType::ProcClipboard;
    Header.Target     = 0;
    Header.Flags      = 0;

    for (auto& [DevID, Instance] : *ActiveInstances) {
        if (Instance.GetFeatureState(FeatureTypes::ClipboardLink, FeatureActionRoute::Outbound)) {
            if (Instance.InstanceSession) {
                Instance.InstanceSession->SessionSend(
                    reinterpret_cast<char*>(Payload.data()),
                    static_cast<int>(Payload.size()),
                    Header
                );
                Logger::log(
                    "Transmit completed for ClipboardManifest promises ({:s}, Size: {:d} bytes, "
                    "Port: {:d}) to "
                    "DeviceID {:d}",
                    CManifest.FormatMime.c_str(),
                    CManifest.TotalSizeBytes,
                    CManifest.ServerPort,
                    static_cast<int>(DevID)
                );
            }
        }
    }
}
