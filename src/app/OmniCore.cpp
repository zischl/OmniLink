#include "OmniCore.h"
#include "Helper.h"
#include "OmniEnums.h"
#include "OmniLogger.h"
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

        break;
    }

    case PayloadType::LinkResponse: {
        DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Event.InstanceIP];

        if (Event.LinkState == NetLinkState::LINKING &&
            InstanceRegistry.GetSessionState(DeviceID)) {

            InstanceRegistry.SetConnectionState(DeviceID, Event.LinkState);

        } else if (
            Event.LinkState == NetLinkState::PENDING && InstanceRegistry.GetSessionState(DeviceID)
        ) {
            RequestHandshake(DeviceID);
        } else if (
            Event.LinkState == NetLinkState::LINKED && InstanceRegistry.GetSessionState(DeviceID)
        ) {

            const Notification UIEvent{
                Alert{
                    "Handshake Complete",
                    "Instance : ",
                    InstanceRegistry.AllInstances[DeviceID].InstanceName
                },
                "HandshakeNotif",
                Notification::EventLayout::BOTTOM_RIGHT,
                2
            };

            PushNotification(UIEvent);
        }

        break;
    }
    case PayloadType::IdentifyResponse: {
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
    const Notification UIEvent{
        Alert{
            "Handshake Request Initiated",
            "Instance : ",
            InstanceRegistry.AllInstances[DeviceID].InstanceName
        },
        "HandshakeNotif",
        Notification::EventLayout::BOTTOM_RIGHT,
        2
    };

    PushNotification(UIEvent);

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

    TransmitNetCommand(DeviceID, Command, 0, OmniNet::Argonized);
}
void OmniCore::HandshakeHandler(HandshakeData Data)
{
    DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Data.IP];

    // Gotta handle ECDH and monitor res later

    InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKED);

    InstanceRegistry.TransmitConnectionState(DeviceID);

    const Notification UIEvent{
        Alert{
            "Handshake Complete",
            "Instance : ",
            InstanceRegistry.AllInstances[DeviceID].InstanceName
        },
        "HandshakeNotif",
        Notification::EventLayout::BOTTOM_RIGHT,
        2
    };

    PushNotification(UIEvent);
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

        std::unique_ptr<OmniNetSession<OmniMTU>> NetSession = SessionManager.Connect(
            InstanceRegistry.UserInstance,
            InstanceRegistry.AllInstances[DeviceID],
            SystemLink.networkPacketHandler,
            &SystemLink.ActiveWindows
        );

        if (NetSession) {
            InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));
            InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING);

            InstanceRegistry.TransmitConnectionState(DeviceID);

            EventData Event{Alert{
                "Establishing Instance Link",
                "Instance : ",
                InstanceRegistry.AllInstances[DeviceID].InstanceName
            }};

            PushNotification(
                Notification{Event, "EstablishingLink", Notification::EventLayout::BOTTOM_RIGHT, 1}
            );
        }

        break;
    }

    case NetLinkState::WAITING: {

        std::unique_ptr<OmniNetSession<OmniMTU>> NetSession = SessionManager.Connect(
            InstanceRegistry.UserInstance,
            InstanceRegistry.AllInstances[DeviceID],
            SystemLink.networkPacketHandler,
            &SystemLink.ActiveWindows
        );

        if (NetSession) {
            InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));
            InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::PENDING);

            InstanceRegistry.TransmitConnectionState(DeviceID);
        }

        EventData Event{Alert{
            "Establishing Instance Link",
            "Instance : ",
            InstanceRegistry.AllInstances[DeviceID].InstanceName
        }};

        PushNotification(
            Notification{Event, "EstablishingLink", Notification::EventLayout::BOTTOM_RIGHT, 1}
        );

        break;
    }
    case LINKING: {
        if (InstanceRegistry.ActiveInstances[DeviceID].InstanceSession) {
            InstanceRegistry.TransmitConnectionState(DeviceID);
        } else {
            std::unique_ptr<OmniNetSession<OmniMTU>> NetSession = SessionManager.Connect(
                InstanceRegistry.UserInstance,
                InstanceRegistry.AllInstances[DeviceID],
                SystemLink.networkPacketHandler,
                &SystemLink.ActiveWindows
            );

            if (NetSession) {
                InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));
                InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING);

                InstanceRegistry.TransmitConnectionState(DeviceID);
            }

            break;
        }
    }
    case LINKED:
        break;
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

        OmniNetCommand command{
            CoreCommandsWArgs::CreateStreamLink,
            2,
            WindowCreationData::Serialize(WGC),
        };

        auto& instance = InstanceRegistry.ActiveInstances[Index];
        if (instance.InstanceSession) {
            TransmitNetCommand(Index, command, 0, OmniNet::Argonized);
            SystemLink.AddCaptureStream(instance.InstanceSession.get(), Index, CaptureMode::DXGI);
        } else {
            EventData Event{Alert{
                "Stream Link Error",
                "Instance : ",
                InstanceRegistry.AllInstances[Index].InstanceName
            }};

            PushNotification(
                Notification{
                    Event,
                    "Please Select An Instance First",
                    Notification::EventLayout::BOTTOM_RIGHT,
                    1
                }
            );
        }

        break;
    }
    case FeatureTypes::WindowLink: {
        WindowCreationData WGC{"Test Window"};

        OmniNetCommand command{
            CoreCommandsWArgs::CreateStreamLink,
            2,
            WindowCreationData::Serialize(WGC),
        };

        std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
        command.Args = payload;
        command.ArgArrayLength = payload.size();

        TransmitNetCommand(Index, command, 0, OmniNet::Argonized);

        auto& instance = InstanceRegistry.ActiveInstances[Index];
        if (instance.InstanceSession) {
            SystemLink.AddCaptureStream(instance.InstanceSession.get(), Index, CaptureMode::WGC);
        } else {
        }

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
