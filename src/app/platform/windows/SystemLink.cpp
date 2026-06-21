#include "SystemLink.h"
#include "WinForge.h"

#include <codecvt>

OmniSystemLink::OmniSystemLink(OmniRenderState& RenderState,
                               std::vector<StreamWindow*>& ActiveWindows)
    : RenderState(RenderState), ActiveWindows(ActiveWindows)
{
}

void OmniSystemLink::SetupSystemLink(HINSTANCE hInstance_,
                                     int nCmdShow_,
                                     HWND WindowID_,
                                     NetworkPacketHandlerFn PacketHandlerFn)
{
    hInstance = hInstance_;
    nCmdShow = nCmdShow_;
    WindowID = WindowID_;

    networkPacketHandler = PacketHandlerFn;
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
OmniSystemLink::AddCaptureStream(session* netSession, DeviceMap targetID, CaptureMode mode)
{
    return StreamController.AddStream(
        RenderState.Device, RenderState.Context, netSession, targetID, mode);
}
