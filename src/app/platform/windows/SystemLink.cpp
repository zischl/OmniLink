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
    auto* window = new WinForge();
    ActiveWindows.push_back(window);
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring WindowTitle =
        converter.from_bytes(reinterpret_cast<const char*>(WindowData.GetTitleU8().data()));
    window->CreateWindowAsync(WindowTitle.c_str(), hInstance, nCmdShow);
    return window;
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
                        AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::DXGI);
                    }
                }
            }
            Logger::log(
                "CaptureStream on ScreenLink started for device {:d}", static_cast<int>(DeviceID)
            );
        } else {
            Logger::log("ScreenLink stopped for device {:d}", static_cast<int>(DeviceID));
        }
    } else {
        if (Action == FeatureAction::Activate) {
            WindowCreationData WindowConfig{"Screen Stream Window"};
            StreamWindow* Window = CreateStreamWindow(WindowConfig);
            Logger::log("StreamWindow created for device {:d}", static_cast<int>(DeviceID));

            OmniNet::PoolConfig Config{};
            if (Window) {
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
            if (!ActiveWindows.empty()) {
                auto* win = ActiveWindows.back();
                ActiveWindows.pop_back();
                delete win;
            }
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
                        AddCaptureStream(Entry->SubStream, DeviceID, CaptureMode::WGC);
                    }
                }
            }
            Logger::log(
                "CaptureStream on WindowLink started for DeviceID {:d}", static_cast<int>(DeviceID)
            );
        } else {
            Logger::log("WindowLink stopped for DeviceID {:d}", static_cast<int>(DeviceID));
        }
    } else {
        if (Action == FeatureAction::Activate) {
            WindowCreationData WGC{"Window Stream Window"};
            StreamWindow* Win = CreateStreamWindow(WGC);
            Logger::log("StreamWindow created for device {:d}", static_cast<int>(DeviceID));

            OmniNet::PoolConfig Config{};
            if (Win) {
                Win->GetFramePool(
                    Config.Data,
                    Config.DataSize,
                    Config.NumSlots,
                    &Config.OnSlotComplete,
                    Config.Ctx
                );
            }
            return Config;
        } else {
            if (!ActiveWindows.empty()) {
                auto* win = ActiveWindows.back();
                ActiveWindows.pop_back();
                delete win;
            }
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
