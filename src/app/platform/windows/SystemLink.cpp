#include "SystemLink.h"
#include "ClipboardTypes.h"
#include "OmniConfig.h"
#include "OmniTCPStream.h"
#include "SessionTypes.h"
#include "WinForge.h"

#include <codecvt>
#include <d3d11.h>

OmniSystemLink::OmniSystemLink(OmniRenderState& RenderState) : RenderState(RenderState) {}

void OmniSystemLink::SetupSystemLink(HINSTANCE hInstance_, int nCmdShow_, HWND WindowID_)
{
    hInstance = hInstance_;
    nCmdShow  = nCmdShow_;
    WindowID  = WindowID_;

    ClipBoardLink::SetPasteRequestCallback(
        [this](const ClipboardManifest& Manifest, UINT Format) -> std::vector<uint8_t> {
            (void)Format;
            if (!ActiveInstances || Manifest.ServerPort == 0 || Manifest.TotalSizeBytes == 0) {
                return {};
            }

            for (auto& [DevID, Instance] : *ActiveInstances) {
                if (Instance.GetFeatureState(
                        FeatureTypes::ClipboardLink, FeatureActionRoute::Inbound
                    )) {
                    auto Stream = std::make_shared<OmniTCPStream>(Manifest.StreamID);
                    if (Stream->Connect(Instance.IPv4_String, Manifest.ServerPort, 5000)) {
                        auto StreamProg = std::make_shared<StreamProgress>();
                        StreamProg->TotalBytes.store(
                            Manifest.TotalSizeBytes, std::memory_order_relaxed
                        );
                        StreamProg->BytesTransferred.store(0, std::memory_order_relaxed);
                        StreamProg->StreamState.store(true, std::memory_order_relaxed);
                        StreamProg->Cancel.store(false, std::memory_order_relaxed);

                        if (ClipboardCtx && ClipboardCtx->OnStreamEvent &&
                            Manifest.TotalSizeBytes > 1048576) {
                            std::string DisplayName = Manifest.Items.empty()
                                                          ? "Clipboard Item"
                                                          : Manifest.Items[0].ItemName;

                            ClipboardStreamEvent CpEvent(
                                Manifest.StreamID,
                                DevID,
                                DisplayName,
                                Manifest.Category == ClipboardCategory::Image
                                    ? "Image"
                                    : (Manifest.Category == ClipboardCategory::FileList ? "File"
                                                                                        : "Text"),
                                Manifest.TotalSizeBytes,
                                false,
                                StreamProg
                            );
                            ClipboardCtx->OnStreamEvent(CpEvent);
                        }

                        std::vector<uint8_t> Buffer;
                        if (Stream->ReceiveToBuffer(
                                Buffer, static_cast<size_t>(Manifest.TotalSizeBytes)
                            )) {
                            StreamProg->BytesTransferred.store(
                                Manifest.TotalSizeBytes, std::memory_order_relaxed
                            );
                            StreamProg->StreamState.store(false, std::memory_order_relaxed);
                            Stream->End();
                            return Buffer;
                        }
                    }
                }
            }
            return {};
        }
    );
}

StreamWindow* OmniSystemLink::CreateStreamWindow(const WindowCreationData& WindowData)
{
    auto* Window   = new WinForge();
    auto  Iterator = std::find(ActiveWindows.begin(), ActiveWindows.end(), nullptr);
    if (Iterator != ActiveWindows.end()) {
        *Iterator = Window;
    } else {
        ActiveWindows.push_back(Window);
    }
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring                                           WindowTitle =
        converter.from_bytes(reinterpret_cast<const char*>(WindowData.GetTitleU8().data()));

    Window->CreateWindowAsync(WindowTitle.c_str(), hInstance, nCmdShow);
    return Window;
}

void OmniSystemLink::ToggleEdgeProbe()
{
    IOCapture.ToggleEdgeProbe(WindowID);
}

void OmniSystemLink::SyncInputFilter()
{
    if (IOCapture.GetEdgeProbeState()) {
        IOShield.InvokeInputFilter();
    } else {
        IOShield.ReleaseInputFilter();
    }
}

OmniStreamController::StreamID OmniSystemLink::AddCaptureStream(
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

    return StreamController.AddStream(
        StreamingDevice.Get(), StreamingContext.Get(), SubStream, DeviceID, Mode, Config
    );
}

void OmniSystemLink::BindIOLinkSession(DeviceMap DeviceID)
{
    if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
        auto& instance = ActiveInstances->at(DeviceID);
        IOCtx.RegisterSession(DeviceID, instance.InstanceSession.get());
    }
}

void OmniSystemLink::UnbindIOLinkSession(DeviceMap DeviceID)
{
    IOCtx.UnregisterSession(DeviceID);
    IOCapture.ConditionManager.Remove(DeviceID);

    if (IOCapture.ConditionManager.Empty() && IOCapture.GetEdgeProbeState()) {
        IOCapture.ToggleEdgeProbe(WindowID);
    }

    SyncInputFilter();
}

OmniNet::PoolConfig OmniSystemLink::SetScreenLinkState(
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    uint16_t           SubStreamID,
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
                    StreamConfig                   Config{};
                    OmniStreamController::StreamID StreamID =
                        AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::DXGI, Config);
                    StreamRegistry[SubStreamID] = StreamID;
                }
            }
            Logger::log(
                "CaptureStream on ScreenLink started for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        } else {
            if (SubStreamID != 0) {
                auto it = StreamRegistry.find(SubStreamID);
                if (it != StreamRegistry.end()) {
                    StreamController.RemoveStream(it->second);
                    StreamRegistry.erase(it);
                }
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t id : Streams) {
                    auto it = StreamRegistry.find(id);
                    if (it != StreamRegistry.end()) {
                        StreamController.RemoveStream(it->second);
                        StreamRegistry.erase(it);
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
            WindowCreationData WindowConfig{"Screen Stream Window"};
            StreamWindow*      Window = CreateStreamWindow(WindowConfig);
            Logger::log(
                "StreamWindow created for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );

            OmniNet::PoolConfig Config{};
            if (Window) {
                if (SubStreamID != 0) {
                    WindowRegistry[SubStreamID] = Window;
                }
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
                auto iter = WindowRegistry.find(SubStreamID);
                if (iter != WindowRegistry.end()) {
                    StreamWindow* Window = iter->second;
                    auto          WindowsIter =
                        std::find(ActiveWindows.begin(), ActiveWindows.end(), Window);
                    if (WindowsIter != ActiveWindows.end()) {
                        *WindowsIter = nullptr;
                    }
                    delete Window;
                    WindowRegistry.erase(iter);
                }
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t id : Streams) {
                    auto iter = WindowRegistry.find(id);
                    if (iter != WindowRegistry.end()) {
                        StreamWindow* Window = iter->second;
                        auto          WindowsIter =
                            std::find(ActiveWindows.begin(), ActiveWindows.end(), Window);
                        if (WindowsIter != ActiveWindows.end()) {
                            *WindowsIter = nullptr;
                        }
                        delete Window;
                        WindowRegistry.erase(iter);
                    }
                }
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
    uint16_t           SubStreamID,
    void*              Context
)
{
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            if (ActiveInstances && ActiveInstances->contains(DeviceID) && SubStreamID != 0) {
                auto&           Instance = ActiveInstances->at(DeviceID);
                SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
                if (Entry && Entry->SubStream) {
                    StreamConfig Config{};
                    if (Context != nullptr) {
                        Config.WindowHandle = reinterpret_cast<HWND>(Context);
                    }
                    OmniStreamController::StreamID StreamID =
                        AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::WGC, Config);
                    StreamRegistry[SubStreamID] = StreamID;
                }
            }
            Logger::log(
                "CaptureStream on WindowLink started for DeviceID {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        } else {
            if (SubStreamID != 0) {
                auto it = StreamRegistry.find(SubStreamID);
                if (it != StreamRegistry.end()) {
                    StreamController.RemoveStream(it->second);
                    StreamRegistry.erase(it);
                }
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::WindowLink);
                for (uint16_t id : Streams) {
                    auto it = StreamRegistry.find(id);
                    if (it != StreamRegistry.end()) {
                        StreamController.RemoveStream(it->second);
                        StreamRegistry.erase(it);
                    }
                }
            }
            Logger::log(
                "WindowLink stopped for DeviceID {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        }
    } else {
        if (Action == FeatureAction::Activate) {
            WindowCreationData WGC{"Window Stream Window"};
            StreamWindow*      Window = CreateStreamWindow(WGC);
            Logger::log(
                "StreamWindow created for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );

            OmniNet::PoolConfig Config{};
            if (Window) {
                if (SubStreamID != 0) {
                    WindowRegistry[SubStreamID] = Window;
                }
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
                auto it = WindowRegistry.find(SubStreamID);
                if (it != WindowRegistry.end()) {
                    StreamWindow* Window = it->second;
                    auto          WindowsIter =
                        std::find(ActiveWindows.begin(), ActiveWindows.end(), Window);
                    if (WindowsIter != ActiveWindows.end()) {
                        *WindowsIter = nullptr;
                    }
                    delete Window;
                    WindowRegistry.erase(it);
                }
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::WindowLink);
                for (uint16_t id : Streams) {
                    auto it = WindowRegistry.find(id);
                    if (it != WindowRegistry.end()) {
                        StreamWindow* Window = it->second;
                        auto          WindowsIter =
                            std::find(ActiveWindows.begin(), ActiveWindows.end(), Window);
                        if (WindowsIter != ActiveWindows.end()) {
                            *WindowsIter = nullptr;
                        }
                        delete Window;
                        WindowRegistry.erase(it);
                    }
                }
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

// Register/Unregister the edge trigger condition for this device and bind/Unbind the net session.
// Setup Edge Probe and Input Shields if not active.. or... remove.
OmniNet::PoolConfig OmniSystemLink::SetInputLinkState(
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    uint16_t           SubStreamID,
    void*              Context
)
{
    (void)SubStreamID;
    (void)Context;
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            IOCapture.AddEdgeCondition(DeviceID);
            BindIOLinkSession(DeviceID);

            if (!IOCapture.GetEdgeProbeState())
                IOCapture.ToggleEdgeProbe(WindowID);

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
    uint16_t           SubStreamID,
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

            if (TargetSubStream) {
                std::lock_guard<std::mutex> Lock(AudioBroadcastMutex);
                ActiveAudioStreams[SubStreamID] = TargetSubStream;

                if (!OmniAudioCapture) {
                    OmniAudioCapture = std::make_unique<AudioCapture>();
                    if (OmniAudioCapture->Init(AudioCaptureMode::DesktopOnly)) {
                        OmniAudioCapture->SetPacketCallback(
                            [this](
                                const uint8_t* Data, size_t Size, const AudioFrameHeader& Header
                            ) {
                                (void)Header;
                                std::lock_guard<std::mutex> BroadcastLock(AudioBroadcastMutex);
                                for (auto& [SubID, SubStream] : ActiveAudioStreams) {
                                    if (SubStream) {
                                        SubStream->ChunkedSend(
                                            reinterpret_cast<CHAR*>(const_cast<uint8_t*>(Data)),
                                            static_cast<int>(Size)
                                        );
                                    }
                                }
                            }
                        );
                        OmniAudioCapture->Start();
                    }
                }
            }
        } else {
            std::lock_guard<std::mutex> Lock(AudioBroadcastMutex);
            if (SubStreamID != 0) {
                ActiveAudioStreams.erase(SubStreamID);
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::AudioLink);
                for (uint16_t id : Streams) {
                    ActiveAudioStreams.erase(id);
                }
            }
            if (ActiveAudioStreams.empty() && OmniAudioCapture) {
                OmniAudioCapture->Stop();
                OmniAudioCapture.reset();
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
                auto it = AudioRenderers.find(SubStreamID);
                if (it != AudioRenderers.end()) {
                    if (it->second) {
                        it->second->Stop();
                    }
                    AudioRenderers.erase(it);
                }
            } else if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::AudioLink);
                for (uint16_t id : Streams) {
                    auto it = AudioRenderers.find(id);
                    if (it != AudioRenderers.end()) {
                        if (it->second) {
                            it->second->Stop();
                        }
                        AudioRenderers.erase(it);
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
    uint16_t           SubStreamID,
    void*              Context
)
{
    (void)SubStreamID;
    if (Context) {
        ClipboardCtx = static_cast<ClipboardFeatureContext*>(Context);
    }
    bool OutboundActive = false;
    if (ActiveInstances) {
        for (const auto& [DevID, Instance] : *ActiveInstances) {
            if (Instance.GetFeatureState(
                    FeatureTypes::ClipboardLink, FeatureActionRoute::Outbound
                )) {
                OutboundActive = true;
                break;
            }
        }
    }

    if (Action == FeatureAction::Activate && Route == FeatureActionRoute::Outbound) {
        if (!ClipboardService.GetState()) {
            ClipboardService.StartMonitoring(
                WindowID,
                [this](const std::string& Text) { TransmitClipboard(Text); },
                [this](const ClipboardManifest& Manifest) { TransmitClipboardManifest(Manifest); }
            );
        }
    } else if (!OutboundActive) {
        ClipboardService.StopMonitoring();
    }

    Logger::log(
        "{:s} ClipboardSync {:s} for DeviceID {:d}",
        Action == FeatureAction::Activate ? "Enabled" : "Disabled",
        Route == FeatureActionRoute::Outbound ? "Outbound" : "Inbound",
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
