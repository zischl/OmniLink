#include "OmniCore.h"
#include "Helper.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "OmniTypes.h"
#include "UIEvents.h"
#include "system_probe_impl.h"
#include <vector>

DeviceMap OmniCore::ActiveIOProcTarget = DeviceMap::C0;
DeviceMap OmniCore::SelectedTargetDevice = DeviceMap::C0;

OmniCore::OmniCore() = default;

void OmniCore::DiscoveryPacketHandler(ProbeEvent Event)
{
    switch (Event.Mode) {
    case PayloadType::LinkRequest: {
        DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Event.InstanceIP];

        InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::WAITING);

        FuncArgTypes Args = ConnectionRequest{DeviceID};
        PushCommandWArgs(Args);
    }

    case PayloadType::LinkResponse: {
        DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Event.InstanceIP];

        if (Event.LinkState == NetLinkState::LINKING &&
            InstanceRegistry.GetSessionState(DeviceID)) {

            InstanceRegistry.SetConnectionState(DeviceID, Event.LinkState);
            Logger::log("Linking Instance @", Event.InstanceIP);

            std::vector<uint8_t> RequestData =
                ConnectionRequest::Serialize(ConnectionRequest{DeviceID});

            RequestHandshake(DeviceID);
        }
    }
    case PayloadType::IdentifyResponse: {
        const Notification UIEvent{Alert{"Instance Found", "bleh"}};
        PushNotification(UIEvent);
        break;
    }

    case PayloadType::DiscoveryRequest:
    case PayloadType::DiscoveryResponse:
    case PayloadType::IdentifyRequest:
        break;
    default:
        break;
    }
}

void OmniCore::ScanInstances()
{
    InstanceRegistry.RefreshInstanceList([this]() -> void { UIState = OmniGUIState::RENDER; });
}
void OmniCore::RequestHandshake(DeviceMap DeviceID)
{
    const HandshakeData Data{
        InstanceRegistry.UserInstance.InstanceIP,
        DeviceID,
        {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
         0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
         0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20},
        HandshakeData::MonitorRes{1920, 1080}
    };

    OmniNetCommand Command{
        CoreCommandsWArgs::InitiateHandshake,
        Variance::GetVariantTypeIndex<HandshakeData, FuncArgTypes>,
        HandshakeData::Serialize(Data)
    };

    TransmitNetCommand(DeviceID, Command);
}
void OmniCore::HandshakeHandler(HandshakeData Data)
{
    DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Data.IP];

    // Gotta handle ECDH and monitor res later

    InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKED);
}

void OmniCore::ConnectInstance(DeviceMap DeviceID)
{
    switch (InstanceRegistry.GetConnectionState(DeviceID))

    {
    case NetLinkState::FAILED:
    case NetLinkState::INACTIVE: {
        if (!SystemLink.networkPacketHandler) {
            return;
        }

        InstanceRegistry.TransmitConnectionRequest(DeviceID);

        std::unique_ptr<session> NetSession = SessionManager.Connect(
            InstanceRegistry.UserInstance,
            InstanceRegistry.ActiveInstances[DeviceID],
            SystemLink.networkPacketHandler,
            &ActiveWindows
        );

        if (NetSession) {
            InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));
            InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING);

            InstanceRegistry.TransmitConnectionState(DeviceID);
        }

        break;
    }

    case NetLinkState::WAITING: {
        std::unique_ptr<session> NetSession = SessionManager.Connect(
            InstanceRegistry.UserInstance,
            InstanceRegistry.ActiveInstances[DeviceID],
            SystemLink.networkPacketHandler,
            &ActiveWindows
        );

        if (NetSession) {
            InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));
            InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING);

            InstanceRegistry.TransmitConnectionState(DeviceID);
        }

        break;
    }
    }
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

        SystemLink.AddCaptureStream(
            InstanceRegistry.ActiveInstances[Index].InstanceSession.get(), Index, CaptureMode::DXGI
        );

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
            InstanceRegistry.ActiveInstances[Index].InstanceSession.get(), Index, CaptureMode::WGC
        );

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
