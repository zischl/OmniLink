#include "SystemLink.h"
#include "CaptureController.h"
#include "LinuxForge.h"

OmniSystemLink::OmniSystemLink(
    RenderState& renderState, CaptureController& captureCtrl, IOLink& inputLink
)
    : render(renderState), capture(captureCtrl), input(inputLink)
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

OmniStreamController::StreamID
OmniSystemLink::AddCaptureStream(OmniNetSubStream* SubStream, DeviceMap DeviceID, CaptureMode Mode)
{
    return StreamController.AddStream(SubStream, DeviceID, Mode);
}

OmniNet::PoolConfig OmniSystemLink::SetScreenLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetWindowLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetInputLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetAudioLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniSystemLink::SetClipboardLinkState(
    DeviceMap DeviceID, FeatureActionRoute Route, FeatureAction Action
)
{
    return OmniNet::PoolConfig{};
}
