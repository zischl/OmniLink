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
    auto* Window = new LinuxForge();
    auto it = std::find(ActiveWindows.begin(), ActiveWindows.end(), nullptr);
    if (it != ActiveWindows.end()) {
        *it = Window;
    } else {
        ActiveWindows.push_back(Window);
    }
    Window->Create();
    (void)info;
    return Window;
}

void OmniSystemLink::ToggleEdgeProbe() {}

void OmniSystemLink::BindSession(DeviceMap DeviceID) { (void)DeviceID; }
void OmniSystemLink::UnbindSession(DeviceMap DeviceID) { (void)DeviceID; }

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
    bool OutboundActive = false;
    if (ActiveInstances) {
        for (const auto& [id, instance] : *ActiveInstances) {
            if (instance.GetFeatureState(
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
                [this](const std::string& Text) { TransmitClipboard(Text); },
                [this](const ClipboardManifest& Manifest) { TransmitClipboardManifest(Manifest); }
            );
        }
    } else if (!OutboundActive) {
        ClipboardService.StopMonitoring();
    }

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
    Header.Target = 0;
    Header.Flags = 0;

    for (auto& [DevID, Instance] : *ActiveInstances) {
        if (Instance.GetFeatureState(FeatureTypes::ClipboardLink, FeatureActionRoute::Outbound)) {
            if (Instance.InstanceSession) {
                Instance.InstanceSession->SessionSend(
                    reinterpret_cast<char*>(Payload.data()),
                    static_cast<int>(Payload.size()),
                    Header
                );
            }
        }
    }
}

void OmniSystemLink::TransmitClipboardManifest(const ClipboardManifest& Manifest)
{
    if (!ActiveInstances)
        return;

    std::vector<uint8_t> Serialized = ClipboardManifest::Serialize(Manifest);
    std::vector<uint8_t> Payload(1 + Serialized.size());
    Payload[0] = static_cast<uint8_t>(ClipboardOp::Manifest);
    std::memcpy(Payload.data() + 1, Serialized.data(), Serialized.size());

    OmniNet::OmniHeader Header;
    Header.PacketType = OmniNet::PacketType::ProcClipboard;
    Header.Target = 0;
    Header.Flags = 0;

    for (auto& [DevID, Instance] : *ActiveInstances) {
        if (Instance.GetFeatureState(FeatureTypes::ClipboardLink, FeatureActionRoute::Outbound)) {
            if (Instance.InstanceSession) {
                Instance.InstanceSession->SessionSend(
                    reinterpret_cast<char*>(Payload.data()),
                    static_cast<int>(Payload.size()),
                    Header
                );
            }
        }
    }
}
