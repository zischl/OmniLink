#include "SystemLink.h"
#include "LinuxForge.h"

SystemLink::SystemLink(RenderState& renderState,
                       CaptureController& captureCtrl,
                       IOLink& inputLink,
                       std::vector<StreamWindow*>& windows)
    : render(renderState)
    , capture(captureCtrl)
    , input(inputLink)
    , activeWindows(windows)
{
}

StreamWindow* SystemLink::CreateStreamWindow(const WindowCreationData& info)
{
    auto* window = new LinuxForge();
    activeWindows.push_back(window);
    window->Create();
    (void)info;
    return window;
}

void SystemLink::ToggleEdgeProbe() {}

void SystemLink::SyncInputFilter() {}

CaptureController::StreamID SystemLink::AddCaptureStream(session* netSession,
                                                         DeviceMap targetID,
                                                         CaptureMode mode)
{
    return capture.AddStream(netSession, targetID, mode);
}
