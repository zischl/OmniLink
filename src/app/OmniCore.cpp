#include "OmniCore.h"
#include "Helper.h"
#include "OmniEnums.h"
#include "OmniLogger.h"
#include "OmniPackets.h"
#include "OmniTypes.h"
#include "SystemLink.h"
#include "UIEvents.h"
#include <vector>

DeviceMap OmniCore::ActiveIOProcTarget = DeviceMap::C0;
DeviceMap OmniCore::SelectedTargetDevice = DeviceMap::C0;

OmniCore::OmniCore()
{
    QryptManager.LoadPairingTokensFromFile();
}

// Callback for the OmniDiscovery class, Ochestrates LinkRequests and LinkResponses
// On the event of simultaneous LinkRequests, the one with the lower IP wins as the initiator
void OmniCore::DiscoveryPacketHandler(ProbeEvent Event)
{
    switch (Event.Mode) {
    case PayloadType::LinkRequest: {
        if (!InstanceRegistry.InstanceLookup.contains(Event.InstanceIP)) {
            Logger::log("LinkRequest From Unknown IP {:x}, Exterminated :]", Event.InstanceIP);
            break;
        }

        DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Event.InstanceIP];

        // Duplicate Request Handling
        // Decides role by IP Battle, winner gets to stay as Initiator else Receiver
        // If lost, Clear ongoing request attempts and session and no more retries.
        // Loser accepts Winners token
        NetLinkState CurrentNetState = InstanceRegistry.GetConnectionState(DeviceID);
        if (CurrentNetState >= NetLinkState::LINKING_INIT &&
            CurrentNetState < NetLinkState::LINKED) {

            if (InstanceRegistry.UserInstance.InstanceIP <
                InstanceRegistry.AllInstances[DeviceID].InstanceIP) {
                Logger::log(
                    "LinkRequest Duel Won, Ignoring Request From : {} ",
                    InstanceRegistry.AllInstances[DeviceID].InstanceName
                );
                break;
            }

            Logger::log(
                "LinkRequest Duel Lost, Awaiting Request From : {}",
                InstanceRegistry.AllInstances[DeviceID].InstanceName
            );
            InstanceRegistry.ResetInstance(DeviceID);
            if (HandshakeRetries.count(DeviceID)) {
                HandshakeRetries.erase(DeviceID);
            }
        }

        InstanceRegistry.SetHandshakeToken(DeviceID, Event.Token);
        InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING_WAIT);

        FuncArgTypes Args = ConnectionRequest{DeviceID};
        PushCommandWArgs(Args);
        NotifyCommandQueue();
        break;
    }

    case PayloadType::LinkResponse: {
        if (!InstanceRegistry.InstanceLookup.contains(Event.InstanceIP)) {
            Logger::log("LinkRequest From Unknown IP {:x}, Exterminated :]", Event.InstanceIP);
            break;
        }

        DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Event.InstanceIP];

        if (Event.Token != InstanceRegistry.GetHandshakeToken(DeviceID)) {
            Logger::log(
                "Token mismatch in LinkResponse for device {} (got {:x}, expected "
                "{:x}), Exterminated :]",
                InstanceRegistry.AllInstances[DeviceID].InstanceName,
                Event.Token,
                InstanceRegistry.GetHandshakeToken(DeviceID)
            );
            break;
        }

        if (!InstanceRegistry.GetSessionState(DeviceID)) {
            Logger::log(
                "LinkResponse for device {} but no active session, Exterminated :]",
                InstanceRegistry.AllInstances[DeviceID].InstanceName
            );
            break;
        }

        // Receiver ready but is he/she/them/it ?
        if (Event.LinkState == NetLinkState::LINKING_WAIT) {
            if (InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING_ACK)) {
                if (HandshakeRetries.count(DeviceID) && HandshakeRetries[DeviceID].Active) {
                    HandshakeRetries[DeviceID].RetriesLeft = 5;
                    HandshakeRetries[DeviceID].NextRetry =
                        std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                }

                RequestHandshake(DeviceID);
            }
        } else if (Event.LinkState == NetLinkState::LINKED) {
            if (InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKED)) {
                if (HandshakeRetries.count(DeviceID)) {
                    HandshakeRetries.erase(DeviceID);
                }

                CancelNotification(DeviceID);

                PushNotification(
                    Notification{
                        Alert{
                            "Handshake Complete",
                            "Instance : ",
                            InstanceRegistry.AllInstances[DeviceID].InstanceName
                        },
                        "HandshakeNotif",
                        Notification::EventLayout::BOTTOM_RIGHT,
                        2
                    }
                );
            }
        } else if (Event.LinkState == NetLinkState::FAILED) {
            InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::FAILED);
            InstanceRegistry.ResetInstance(DeviceID);

            if (HandshakeRetries.count(DeviceID)) {
                HandshakeRetries.erase(DeviceID);
            }

            QryptManager.ClearSession(DeviceID);

            CancelNotification(DeviceID);

            PushNotification(
                Notification{
                    Alert{
                        "Connection Rejected",
                        "Instance : ",
                        InstanceRegistry.AllInstances[DeviceID].InstanceName
                    },
                    "ConnectFail",
                    Notification::EventLayout::BOTTOM_RIGHT,
                    3
                }
            );
        }
        break;
    }

    case PayloadType::IdentifyResponse:
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

// Why skip 0 ? null init is.. 0
uint32_t OmniCore::GenerateHandshakeToken()
{
    static std::random_device RandomDevice;
    static std::mt19937 Generator(RandomDevice());
    std::uniform_int_distribution<uint32_t> Distribution(1, UINT32_MAX);
    return Distribution(Generator);
}

// Literally ordering the reciever side's command queue to call HandshakeHandler
void OmniCore::RequestHandshake(DeviceMap DeviceID)
{

    uint32_t HandshakeToken = InstanceRegistry.GetHandshakeToken(DeviceID);
    std::vector<uint8_t> LocalPublicKey = QryptManager.GenerateKeyPair(DeviceID);

    HandshakeData Data{
        InstanceRegistry.UserInstance.InstanceIP,
        DeviceID,
        HandshakeToken,
        {},
        HandshakeData::MonitorRes{1920, 1080}
    };

    if (LocalPublicKey.size() == 32) {
        std::copy_n(LocalPublicKey.data(), 32, Data.Key);
    }

    OmniNetCommand Command{
        CoreCommandsWArgs::InitiateHandshake,
        Variance::GetVariantTypeIndex<HandshakeData, FuncArgTypes>,
        HandshakeData::Serialize(Data)
    };

    TransmitNetCommand(DeviceID, Command, 0, OmniNet::Argonized);

    HandshakeRetries[DeviceID] = HandshakeRetryContext{
        DeviceID,
        HandshakeToken,
        1,
        std::chrono::steady_clock::now() + std::chrono::seconds(5),
        true
    };
}

// Generating key pairs, deriving shared secret, handling auth
// If receiver lookup auth tokens in either memory or storage otherwise ask for approval
// Both sides will display the passkey for approval if required
// HandshakeRetries will not be recalling a failed auth.
void OmniCore::HandshakeHandler(HandshakeData Data)
{
    if (!InstanceRegistry.InstanceLookup.contains(Data.IP)) {
        Logger::log("LinkRequest From Unknown IP {:x}, Exterminated :]", Data.IP);
        return;
    }

    DeviceMap DeviceID = InstanceRegistry.InstanceLookup[Data.IP];

    if (Data.Token != InstanceRegistry.GetHandshakeToken(DeviceID)) {
        Logger::log(
            "Token mismatch in HandshakeData for device {} (got {:x}, expected {:x}), discarding",
            InstanceRegistry.AllInstances[DeviceID].InstanceName,
            Data.Token,
            InstanceRegistry.GetHandshakeToken(DeviceID)
        );
        return;
    }

    NetLinkState CurrentLinkState = InstanceRegistry.GetConnectionState(DeviceID);
    if (CurrentLinkState < NetLinkState::LINKING_WAIT || CurrentLinkState >= NetLinkState::LINKED) {
        Logger::log(
            "HandshakeData for device {} in unexpected state {}, Eliminated :]",
            InstanceRegistry.AllInstances[DeviceID].InstanceName,
            static_cast<int>(CurrentLinkState)
        );
        return;
    }

    bool PriorSessionAuth = QryptManager.SessionAuthState(DeviceID);

    // ECDH plus Passkey..
    std::vector<uint8_t> LocalPubKey = QryptManager.GenerateKeyPair(DeviceID);
    QryptManager.DeriveSecret(DeviceID, Data.Key, 32);
    int32_t PassKey = QryptManager.GeneratePasskey(DeviceID);

    Logger::log(
        "Derived ECDH Shared Secret for device {}, PassKey: {:06d}",
        InstanceRegistry.AllInstances[DeviceID].InstanceName,
        PassKey
    );

    bool ResponseRequired = (CurrentLinkState == NetLinkState::LINKING_WAIT);
    if (ResponseRequired) {
        HandshakeData HandshakeResponse{
            InstanceRegistry.UserInstance.InstanceIP,
            DeviceID,
            Data.Token,
            {},
            HandshakeData::MonitorRes{1920, 1080}
        };
        if (LocalPubKey.size() == 32) {
            std::copy_n(LocalPubKey.data(), 32, HandshakeResponse.Key);
        }

        OmniNetCommand Command{
            CoreCommandsWArgs::InitiateHandshake,
            Variance::GetVariantTypeIndex<HandshakeData, FuncArgTypes>,
            HandshakeData::Serialize(HandshakeResponse)
        };
        TransmitNetCommand(DeviceID, Command, 0, OmniNet::Argonized);
    }

    InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING_AUTH);

    // If this is the receiver...
    if (ResponseRequired) {
        // Yes.. finally.. AUTH
        if (PriorSessionAuth) {
            Logger::log(
                "Device {} active in-memory session found, auto-approving non-permanent "
                "reconnection",
                InstanceRegistry.AllInstances[DeviceID].InstanceName
            );
            AcceptConnection(DeviceID, false);
            return;
        } else if (QryptManager.PairingTokenState(DeviceID)) {
            Logger::log(
                "Device {} is trusted permanently on disk, auto-approving reconnection",
                InstanceRegistry.AllInstances[DeviceID].InstanceName
            );
            AcceptConnection(DeviceID, true);
            return;
        } else {
            HandshakeConfirmEvent Event{DeviceID};
            snprintf(Event.VerificationCode, sizeof(Event.VerificationCode), "%06d", PassKey);
            snprintf(
                Event.InstanceName,
                sizeof(Event.InstanceName),
                "%s",
                InstanceRegistry.AllInstances[DeviceID].InstanceName
            );
            PushNotification(
                DeviceID,
                Notification{Event, "HandshakeEvent", Notification::EventLayout::CENTER, 30.0f}
            );
        }

    } else {
        HandshakeWaitEvent Event{.DeviceID = DeviceID};
        snprintf(Event.VerificationCode, sizeof(Event.VerificationCode), "%06d", PassKey);
        snprintf(
            Event.InstanceName,
            sizeof(Event.InstanceName),
            "%s",
            InstanceRegistry.AllInstances[DeviceID].InstanceName
        );
        PushNotification(
            DeviceID,
            Notification{Event, "HandshakeEvent", Notification::EventLayout::CENTER, 30.0f}
        );
    }

    // Set 30 second user interaction timeout deadline
    if (HandshakeRetries.count(DeviceID)) {
        HandshakeRetries[DeviceID].NextRetry =
            std::chrono::steady_clock::now() + std::chrono::seconds(30);
        HandshakeRetries[DeviceID].RetriesLeft = 1;
    }
}

void OmniCore::AcceptConnection(DeviceMap DeviceID, bool TrustPermanently)
{
    QryptManager.AuthenticateSession(DeviceID);
    InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKED);
    InstanceRegistry.TransmitConnectionState(DeviceID);

    if (TrustPermanently) {
        auto Token = QryptManager.GeneratePairingToken(DeviceID);
        QryptManager.StorePairingToken(DeviceID, Token);
    }

    if (HandshakeRetries.count(DeviceID)) {
        HandshakeRetries.erase(DeviceID);
    }
}

void OmniCore::RejectConnection(DeviceMap DeviceID)
{
    QryptManager.ClearSession(DeviceID);
    InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::FAILED);
    InstanceRegistry.TransmitConnectionState(DeviceID);

    if (HandshakeRetries.count(DeviceID)) {
        HandshakeRetries.erase(DeviceID);
    }

    InstanceRegistry.ResetInstance(DeviceID);
}

void OmniCore::ConnectInstance(DeviceMap DeviceID)
{
    switch (InstanceRegistry.GetConnectionState(DeviceID)) {

    case NetLinkState::FAILED:
    case NetLinkState::INACTIVE:
        // New Connection
        {
            const uint32_t HandshakeToken = GenerateHandshakeToken();
            InstanceRegistry.SetHandshakeToken(DeviceID, HandshakeToken);

            std::unique_ptr<OmniNetSession<OmniMTU>> NetSession =
                SessionManager.Connect<NetworkPacketHandler>(
                    InstanceRegistry.UserInstance,
                    InstanceRegistry.AllInstances[DeviceID],
                    &SystemLink.ActiveWindows
                );

            if (!NetSession) {
                InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::FAILED);
                Logger::log(
                    "ConnectInstance: OmniNetSession Creation Failed : {}",
                    InstanceRegistry.AllInstances[DeviceID].InstanceName
                );
                PushNotification(
                    Notification{
                        Alert{
                            "Connection Failed",
                            "Instance : ",
                            InstanceRegistry.AllInstances[DeviceID].InstanceName
                        },
                        "ConnectFail",
                        Notification::EventLayout::BOTTOM_RIGHT,
                        2
                    }
                );
                return;
            }

            InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));
            InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::LINKING_INIT);

            InstanceRegistry.TransmitConnectionRequest(DeviceID, HandshakeToken);

            // Retry until receiver side sends LINKING_WAIT, max 5 tries tho
            HandshakeRetries[DeviceID] = HandshakeRetryContext{
                DeviceID,
                HandshakeToken,
                5,
                std::chrono::steady_clock::now() + std::chrono::milliseconds(500),
                true
            };

            PushNotification(
                Notification{
                    Alert{
                        "Establishing Instance Link",
                        "Instance : ",
                        InstanceRegistry.AllInstances[DeviceID].InstanceName
                    },
                    "EstablishingLink",
                    Notification::EventLayout::BOTTOM_RIGHT,
                    1
                }
            );
            break;
        }

    case NetLinkState::LINKING_WAIT:
        // Incomming Request
        {
            const uint32_t HandshakeToken = InstanceRegistry.GetHandshakeToken(DeviceID);

            std::unique_ptr<OmniNetSession<OmniMTU>> NetSession =
                SessionManager.Connect<NetworkPacketHandler>(
                    InstanceRegistry.UserInstance,
                    InstanceRegistry.AllInstances[DeviceID],
                    &SystemLink.ActiveWindows
                );

            if (!NetSession) {
                InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::FAILED);
                Logger::log(
                    "ConnectInstance: OmniNetSession Creation Failed : {}",
                    InstanceRegistry.AllInstances[DeviceID].InstanceName
                );
                PushNotification(
                    Notification{
                        Alert{
                            "Connection Failed",
                            "Instance : ",
                            InstanceRegistry.AllInstances[DeviceID].InstanceName
                        },
                        "ConnectFail",
                        Notification::EventLayout::BOTTOM_RIGHT,
                        2
                    }
                );
                return;
            }

            InstanceRegistry.ActivateInstance(DeviceID, std::move(NetSession));
            InstanceRegistry.TransmitConnectionState(DeviceID);

            // Spam LINKING_WAIT until the initiator sends HandshakeData, still max 5 tries tho
            HandshakeRetries[DeviceID] = HandshakeRetryContext{
                DeviceID,
                HandshakeToken,
                5,
                std::chrono::steady_clock::now() + std::chrono::milliseconds(1000),
                true
            };

            PushNotification(
                Notification{
                    Alert{
                        "Establishing Instance Link",
                        "Instance : ",
                        InstanceRegistry.AllInstances[DeviceID].InstanceName
                    },
                    "EstablishingLink",
                    Notification::EventLayout::BOTTOM_RIGHT,
                    1
                }
            );
            break;
        }

    case NetLinkState::LINKING_INIT:
    case NetLinkState::LINKING_ACK:
    case NetLinkState::LINKED:
        break;
    }
}

void OmniCore::FailHandshake(DeviceMap DeviceID, const char* Reason)
{
    Logger::log(
        "Handshake failed for device {}: {}",
        InstanceRegistry.AllInstances[DeviceID].InstanceName,
        Reason
    );
    InstanceRegistry.SetConnectionState(DeviceID, NetLinkState::FAILED);

    PushNotification(
        Notification{
            Alert{
                "Connection Failed",
                "Instance : ",
                InstanceRegistry.AllInstances[DeviceID].InstanceName
            },
            "ConnectFail",
            Notification::EventLayout::BOTTOM_RIGHT,
            3
        }
    );

    InstanceRegistry.ResetInstance(DeviceID);
}

void OmniCore::HandleHandshakeRetries()
{
    const auto now = std::chrono::steady_clock::now();

    std::vector<DeviceMap> ExterminationTargets;

    for (auto& [DeviceID, RetryContext] : HandshakeRetries) {
        if (!RetryContext.Active || now < RetryContext.NextRetry) {
            continue;
        }

        if (RetryContext.RetriesLeft <= 0) {
            FailHandshake(DeviceID, "Handshake Response Timed Out");
            ExterminationTargets.push_back(DeviceID);
            continue;
        }

        const NetLinkState state = InstanceRegistry.GetConnectionState(DeviceID);

        switch (state) {
        case NetLinkState::LINKING_INIT:
            // Spam ConnectionRequest until LINKING_WAIT
            InstanceRegistry.TransmitConnectionRequest(DeviceID, RetryContext.Token);
            InstanceRegistry.TransmitConnectionState(DeviceID);
            --RetryContext.RetriesLeft;
            RetryContext.NextRetry = now + std::chrono::milliseconds(500);
            break;

        case NetLinkState::LINKING_WAIT:
            // Spam LINKING_WAIT until a HandshakeRequest gets here
            InstanceRegistry.TransmitConnectionState(DeviceID);
            --RetryContext.RetriesLeft;
            RetryContext.NextRetry = now + std::chrono::milliseconds(1000);
            break;

        case NetLinkState::LINKING_ACK:
            // Spam HandshakeRequest till Linked
            RequestHandshake(DeviceID);
            --RetryContext.RetriesLeft;
            RetryContext.NextRetry = now + std::chrono::seconds(5);
            break;

        case NetLinkState::LINKING_AUTH:
            //  Just.. press... something..
            FailHandshake(DeviceID, "Handshake Timed Out Waiting for User Approval");
            ExterminationTargets.push_back(DeviceID);
            break;

        default:
            // At this point it better be LINKED
            ExterminationTargets.push_back(DeviceID);
            continue;
        }
    }

    for (DeviceMap DeviceID : ExterminationTargets) {
        HandshakeRetries.erase(DeviceID);
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

        if (InstanceRegistry.ActiveInstances.contains(Index)) {
            auto& Instance = InstanceRegistry.ActiveInstances.at(Index);
            if (Instance.InstanceSession) {
                TransmitNetCommand(Index, command, 0, OmniNet::Argonized);
                Sleep(500);
                SystemLink.AddCaptureStream(
                    Instance.InstanceSession.get(), Index, CaptureMode::DXGI
                );
            } else {
                PushNotification(
                    Notification{
                        Alert{
                            "Stream Link Error",
                            "Instance : ",
                            InstanceRegistry.AllInstances[Index].InstanceName
                        },
                        "Please Select An Instance First",
                        Notification::EventLayout::BOTTOM_RIGHT,
                        1
                    }
                );
            }
        } else {
            PushNotification(
                Notification{
                    Alert{
                        "Stream Link Error",
                        "Instance : ",
                        InstanceRegistry.AllInstances[Index].InstanceName
                    },
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
        Sleep(500);

        if (InstanceRegistry.ActiveInstances.contains(Index)) {
            auto& Instance = InstanceRegistry.ActiveInstances.at(Index);
            if (Instance.InstanceSession) {
                SystemLink.AddCaptureStream(
                    Instance.InstanceSession.get(), Index, CaptureMode::WGC
                );
            }
        } else {
            PushNotification(
                Notification{
                    Alert{
                        "Stream Link Error",
                        "Instance : ",
                        InstanceRegistry.AllInstances[Index].InstanceName
                    },
                    "Please Select An Instance First",
                    Notification::EventLayout::BOTTOM_RIGHT,
                    1
                }
            );
        }

        break;
    }

    case FeatureTypes::InputLink:
        SystemLink.ToggleEdgeProbe(InstanceRegistry.ActiveInstances);
        SystemLink.SyncInputFilter();
        break;

    case FeatureTypes::AudioLink:
        break;
    }
}
