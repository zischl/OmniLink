#include "SystemLink.h"
#include "WinForge.h"

OmniSystemLink::OmniSystemLink(OmniRenderState& RenderState,
                               std::vector<StreamWindow*>& ActiveWindows)
    : RenderState(RenderState), ActiveWindows(ActiveWindows)
{
}

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
    window->CreateWindowAsync(WindowData.GetTitleW().c_str(), hInstance, nCmdShow);
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
OmniSystemLink::AddCaptureStream(session* netSession, DeviceMap targetID, CaptureMode mode)
{
    return StreamController.AddStream(
        RenderState.Device, RenderState.Context, netSession, targetID, mode);
}
