#include "OmniCore.h"
#include "Helper.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "OmniTypes.h"
#include <vector>

DeviceMap OmniCore::ActiveIOProcTarget = DeviceMap::C0;
DeviceMap OmniCore::SelectedTargetDevice = DeviceMap::C0;

OmniCore::OmniCore() = default;

void OmniCore::DiscoveryPacketHandler(ProbeEvent Event)
{
    switch (Event.Mode) {
    case PayloadType::LinkRequest: {
        DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Event.InstanceIP];

        FuncArgTypes Args = ConnectionRequest{DeviceID};
        PushCommandWArgs(Args);
    }

    case PayloadType::LinkResponse: {
        DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Event.InstanceIP];

        InstanceRegistry.SetConnectionState(DeviceID,
                                            Event.Flags == ProbeEventFlags::Succeeded
                                                ? NetLinkState::LINKED
                                                : NetLinkState::FAILED);
    }
    }
}

void OmniCore::ScanInstances()
{
    InstanceRegistry.RefreshInstanceList([this]() -> void { UIState = OmniGUIState::RENDER; });
}

void OmniCore::ConnectInstance(DeviceMap DeviceID)
{
    if (!SystemLink.networkPacketHandler) {
        return;
    }

    std::unique_ptr<session> NetSession =
        SessionManager.Connect(InstanceRegistry.UserInstance,
                               InstanceRegistry.ActiveInstances[DeviceID],
                               SystemLink.networkPacketHandler,
                               &ActiveWindows);

    InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));

    InstanceRegistry.TransmitConnectionState(DeviceID, NetLinkState::LINKED);
}

void OmniCore::InitiateLinkingSequence(DeviceMap DeviceID)
{
    InstanceRegistry.TransmitConnectionRequest(DeviceID);

    ConnectInstance(DeviceID);

    InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING);
}

void OmniCore::SwapInstanceLayout(int DeviceID1, int DeviceID2)
{
    InstanceRegistry.SwapInstances(DeviceMap(DeviceID1), DeviceMap(DeviceID2));
}

void OmniCore::CreateStreamLink(WindowCreationData& WindowInfo)
{
    SystemLink.CreateStreamWindow(WindowInfo);
}

void OmniCore::ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index)
{

    switch (FeatureIndex) {
    case FeatureTypes::ScreenLink: {
        WindowCreationData WGC{"Test Window"};

        OmniNetCommand command{};
        command.CommandType = CoreCommandsWArgs::CreateStreamLink;
        command.ArgTypeIndex = 2;

        std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
        command.Args = payload;
        command.ArgArrayLength = payload.size();

        TransmitNetCommand(Index, command, 0, OmniNet::Argonized);

        SystemLink.AddCaptureStream(InstanceRegistry.ActiveInstances[Index].InstanceSession.get(),
                                    Index,
                                    CaptureMode::DXGI);

        break;
    }
    case FeatureTypes::WindowLink: {
        WindowCreationData WGC{"Test Window"};

        OmniNetCommand command{};
        command.CommandType = CoreCommandsWArgs::CreateStreamLink;
        command.ArgTypeIndex = 2;

        std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
        command.Args = payload;
        command.ArgArrayLength = payload.size();

        TransmitNetCommand(Index, command, 0, OmniNet::Argonized);

        SystemLink.AddCaptureStream(
            InstanceRegistry.ActiveInstances[Index].InstanceSession.get(), Index, CaptureMode::WGC);

        break;
    }

    case FeatureTypes::InputLink:
        SystemLink.ToggleEdgeProbe(InstanceRegistry.ActiveInstances);
        SystemLink.SyncInputFilter();
        break;

    case FeatureTypes::AudioLink: {
        break;
    }
    }
}
