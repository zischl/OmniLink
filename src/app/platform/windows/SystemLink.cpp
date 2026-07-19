#include "SystemLink.h"
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

OmniStreamController::StreamID OmniSystemLink::AddCaptureStream(
    OmniNetSession<OmniMTU>* netSession, DeviceMap targetID, CaptureMode mode
)
{
    if (!StreamingDevice) {
        D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            creationFlags,
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
        StreamingDevice.Get(), StreamingContext.Get(), netSession, targetID, mode
    );
}
