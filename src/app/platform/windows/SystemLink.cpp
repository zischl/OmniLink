#include "SystemLink.h"
#include "SessionTypes.h"
#include "WinForge.h"

#include <codecvt>
#include <d3d11.h>

OmniSystemLink::OmniSystemLink(OmniRenderState& RenderState) : RenderState(RenderState) {}

void OmniSystemLink::SetupSystemLink(HINSTANCE hInstance_, int nCmdShow_, HWND WindowID_)
{
    hInstance = hInstance_;
    nCmdShow = nCmdShow_;
    WindowID = WindowID_;
}

StreamWindow* OmniSystemLink::CreateStreamWindow(const WindowCreationData& WindowData)
{
    auto* Window = new WinForge();
    auto iter = std::find(ActiveWindows.begin(), ActiveWindows.end(), nullptr);
    if (iter != ActiveWindows.end()) {
        *iter = Window;
    } else {
        ActiveWindows.push_back(Window);
    }
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring WindowTitle =
        converter.from_bytes(reinterpret_cast<const char*>(WindowData.GetTitleU8().data()));
    Window->CreateWindowAsync(WindowTitle.c_str(), hInstance, nCmdShow);
    return Window;
}

void OmniSystemLink::ToggleEdgeProbe(ActiveInstanceContainer& ActiveInstances)
{
    IOCapture.ToggleEdgeProbe(WindowID, ActiveInstances);
}

void OmniSystemLink::SyncInputFilter()
{
    if (IOCapture.GetEdgeProbeState()) {
        IOShield.InvokeInputFilter();
    } else {
        IOShield.ReleaseInputFilter();
    }
}

OmniStreamController::StreamID
OmniSystemLink::AddCaptureStream(OmniNetSubStream* SubStream, DeviceMap DeviceID, CaptureMode Mode)
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
        StreamingDevice.Get(), StreamingContext.Get(), SubStream, DeviceID, Mode
    );
}

OmniNet::PoolConfig OmniSystemLink::SetScreenLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                uint16_t SubStreamID =
                    Instance.GetFirstSubStreamForFeature(FeatureTypes::ScreenLink);
                if (SubStreamID != 0) {
                    SubStreamEntry* Entry = Instance.FindSubStream(SubStreamID);
                    if (Entry && Entry->SubStream) {
                        OmniStreamController::StreamID StreamID =
                            AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::DXGI);
                        StreamRegistry.insert({{DeviceID, FeatureTypes::ScreenLink}, StreamID});
                    }
                }
            }
            Logger::log(
                "CaptureStream on ScreenLink started for device {:d}", static_cast<int>(DeviceID)
            );
        } else {
            auto Range = StreamRegistry.equal_range({DeviceID, FeatureTypes::ScreenLink});
            for (auto iter = Range.first; iter != Range.second; ++iter) {
                StreamController.RemoveStream(iter->second);
            }
            StreamRegistry.erase(Range.first, Range.second);
            Logger::log("ScreenLink stopped for device {:d}", static_cast<int>(DeviceID));
        }
    } else {
        if (Action == FeatureAction::Activate) {
            WindowCreationData WindowConfig{"Screen Stream Window"};
            StreamWindow* Window = CreateStreamWindow(WindowConfig);
            Logger::log("StreamWindow created for device {:d}", static_cast<int>(DeviceID));

            OmniNet::PoolConfig Config{};
            if (Window) {
                WindowRegistry.insert({{DeviceID, FeatureTypes::ScreenLink}, Window});
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
            auto Range = WindowRegistry.equal_range({DeviceID, FeatureTypes::ScreenLink});
            for (auto iter = Range.first; iter != Range.second; ++iter) {
                StreamWindow* Window = iter->second;
                auto WindowsIter = std::find(ActiveWindows.begin(), ActiveWindows.end(), Window);
                if (WindowsIter != ActiveWindows.end()) {
                    *WindowsIter = nullptr;
                }
                delete Window;
            }
            WindowRegistry.erase(Range.first, Range.second);
            Logger::log("StreamWindow closed for device {:d}", static_cast<int>(DeviceID));
        }
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetWindowLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            if (ActiveInstances && ActiveInstances->contains(DeviceID)) {
                auto& Instance = ActiveInstances->at(DeviceID);
                uint16_t SubStreamID =
                    Instance.GetFirstSubStreamForFeature(FeatureTypes::WindowLink);
                if (SubStreamID != 0) {
                    SubStreamEntry* Entry = Instance.FindSubStream(SubStreamID);
                    if (Entry && Entry->SubStream) {
                        OmniStreamController::StreamID StreamID =
                            AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::WGC);
                        StreamRegistry.insert({{DeviceID, FeatureTypes::WindowLink}, StreamID});
                    }
                }
            }
            Logger::log(
                "CaptureStream on WindowLink started for DeviceID {:d}", static_cast<int>(DeviceID)
            );
        } else {
            auto Range = StreamRegistry.equal_range({DeviceID, FeatureTypes::WindowLink});
            for (auto iter = Range.first; iter != Range.second; ++iter) {
                StreamController.RemoveStream(iter->second);
            }
            StreamRegistry.erase(Range.first, Range.second);
            Logger::log("WindowLink stopped for DeviceID {:d}", static_cast<int>(DeviceID));
        }
    } else {
        if (Action == FeatureAction::Activate) {
            WindowCreationData WGC{"Window Stream Window"};
            StreamWindow* Window = CreateStreamWindow(WGC);
            Logger::log("StreamWindow created for device {:d}", static_cast<int>(DeviceID));

            OmniNet::PoolConfig Config{};
            if (Window) {
                WindowRegistry.insert({{DeviceID, FeatureTypes::WindowLink}, Window});
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
            auto Range = WindowRegistry.equal_range({DeviceID, FeatureTypes::WindowLink});
            for (auto iter = Range.first; iter != Range.second; ++iter) {
                StreamWindow* Window = iter->second;
                auto WindowsIter = std::find(ActiveWindows.begin(), ActiveWindows.end(), Window);
                if (WindowsIter != ActiveWindows.end()) {
                    *WindowsIter = nullptr;
                }
                delete Window;
            }
            WindowRegistry.erase(Range.first, Range.second);
            Logger::log("StreamWindow closed for device {:d}", static_cast<int>(DeviceID));
        }
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetInputLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    if (Route == FeatureActionRoute::Outbound) {
        SyncInputFilter();
        Logger::log(
            "{:s} IOLink for DeviceID {:d}",
            Action == FeatureAction::Activate ? "Enabled" : "Disabled",
            static_cast<int>(DeviceID)
        );
    } else {
        Logger::log(
            "{:s} InputSynth for DeviceID {:d}",
            Action == FeatureAction::Activate ? "Enabled" : "Disabled",
            static_cast<int>(DeviceID)
        );
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetAudioLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    if (Route == FeatureActionRoute::Outbound) {
        Logger::log(
            "{:s} AudioLink for DeviceID {:d}",
            Action == FeatureAction::Activate ? "Starting" : "Stopping",
            static_cast<int>(DeviceID)
        );
    } else {
        Logger::log(
            "{:s} AuidoLink for DeviceID {:d}",
            Action == FeatureAction::Activate ? "Starting" : "Stopping",
            static_cast<int>(DeviceID)
        );
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetClipboardLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    Logger::log(
        "{:s} ClipboardSync {:s} for DeviceID {:d}",
        Action == FeatureAction::Activate ? "Enabled" : "Disabled",
        Route == FeatureActionRoute::Outbound ? "Outbound" : "Inbound",
        static_cast<int>(DeviceID)
    );
    return OmniNet::PoolConfig{};
}
