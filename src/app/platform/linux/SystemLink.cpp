#include "SystemLink.h"
#include "LinuxForge.h"

OmniSystemLink::OmniSystemLink(RenderState& renderState,
                               CaptureController& captureCtrl,
                               IOLink& inputLink)
    : render(renderState)
    , capture(captureCtrl)
    , input(inputLink)
{
}

StreamWindow* OmniSystemLink::CreateStreamWindow(const WindowCreationData& info)
{
    auto* window = new LinuxForge();
    ActiveWindows.push_back(window);
    window->Create();
    (void)info;
    return window;
}

void OmniSystemLink::ToggleEdgeProbe() {}

void OmniSystemLink::SyncInputFilter() {}

CaptureController::StreamID OmniSystemLink::AddCaptureStream(session* netSession,
                                                         DeviceMap targetID,
                                                         CaptureMode mode)
{
    return capture.AddStream(netSession, targetID, mode);
}
