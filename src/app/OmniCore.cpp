#include "OmniCore.h"
#include "Helper.h"
#include "OmniEnums.h"
#include "OmniInstances.h"
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
    SystemLink.ActiveInstances = &InstanceRegistry.ActiveInstances;
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
        PushCommandWArgs(DeviceID, Args);
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
                QryptManager.AuthenticateSession(DeviceID);

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

typedef OmniNet::PoolConfig (OmniSystemLink::*FeatureHandlerFn)(
    DeviceMap, FeatureActionRoute, FeatureAction
);

static const std::unordered_map<FeatureTypes, FeatureHandlerFn> FeatureDispatchTable = {
    {FeatureTypes::ScreenLink, &OmniSystemLink::SetScreenLinkState},
    {FeatureTypes::WindowLink, &OmniSystemLink::SetWindowLinkState},
    {FeatureTypes::InputLink, &OmniSystemLink::SetInputLinkState},
    {FeatureTypes::AudioLink, &OmniSystemLink::SetAudioLinkState},
    {FeatureTypes::ClipboardLink, &OmniSystemLink::SetClipboardLinkState}
};

OmniNet::PoolConfig OmniCore::DispatchFeatureState(
    FeatureTypes Feature, DeviceMap Device, FeatureActionRoute Route, FeatureAction Action
)
{
    auto iter = FeatureDispatchTable.find(Feature);
    if (iter != FeatureDispatchTable.end() && iter->second) {
        return (SystemLink.*(iter->second))(Device, Route, Action);
    }
    return OmniNet::PoolConfig{};
}

OmniNet::PoolConfig OmniCore::UpdateFeatureState(
    DeviceMap Device, FeatureTypes Feature, FeatureActionRoute Route, FeatureAction Action
)
{
    if (!InstanceRegistry.ActiveInstances.contains(Device)) {
        return OmniNet::PoolConfig{};
    }

    auto& Instance = InstanceRegistry.ActiveInstances.at(Device);
    Instance.SetFeatureState(Feature, Route, Action == FeatureAction::Activate);

    return DispatchFeatureState(Feature, Device, Route, Action);
}

void OmniCore::ToggleFeature(FeatureTypes FeatureIndex, DeviceMap DeviceID)
{
    if (DeviceID == DeviceMap::C0 || !InstanceRegistry.ActiveInstances.contains(DeviceID)) {
        std::string instName = InstanceRegistry.AllInstances.contains(DeviceID)
                                   ? InstanceRegistry.AllInstances[DeviceID].InstanceName
                                   : "Unknown";
        PushNotification(
            Notification{
                Alert{"Stream Link Error", "Instance : ", instName},
                "Please Select An Instance First",
                Notification::EventLayout::BOTTOM_RIGHT,
                1
            }
        );
        return;
    }

    auto& Instance = InstanceRegistry.ActiveInstances.at(DeviceID);
    bool FeatureState = Instance.GetFeatureState(FeatureIndex, FeatureActionRoute::Outbound);
    FeatureAction TargetAction = FeatureState ? FeatureAction::Deactivate : FeatureAction::Activate;

    FeatureToggleData ToggleData{FeatureIndex, TargetAction};

    const bool SubStreamRequired =
        (FeatureIndex == FeatureTypes::ScreenLink || FeatureIndex == FeatureTypes::WindowLink ||
         FeatureIndex == FeatureTypes::AudioLink);

    if (SubStreamRequired) {
        if (TargetAction == FeatureAction::Activate) {
            OmniNetSubStream* SubStream = Instance.InstanceSession->OpenSubStream();
            if (SubStream) {
                const uint16_t ID =
                    OmniActiveInstance::NextSubStreamID.fetch_add(1, std::memory_order_relaxed);
                Instance.SubStreamRegistry[ID] = SubStreamEntry{SubStream, SubStreamState::Pending};
                Instance.RegisterFeatureSubStream(FeatureIndex, ID);

                ToggleData.SubStreamID = ID;

                SubStreamData CreateData{SubStreamAction::Create, ID, SubStream->GetLocalPort()};
                OmniNetCommand CreateCmd{
                    CoreCommandsWArgs::SubStream,
                    Variance::GetVariantTypeIndex<SubStreamData, FuncArgTypes>,
                    SubStreamData::Serialize(CreateData)
                };
                TransmitNetCommand(DeviceID, CreateCmd, 0, OmniNet::Argonized);

                Logger::log(
                    "Feature {:d} Outbound Activate, SubStreamID={:d} Port={:d}",
                    static_cast<int>(FeatureIndex),
                    ID,
                    SubStream->GetLocalPort()
                );
            } else {
                Logger::log("No free SubStream slots for device {:d}", static_cast<int>(DeviceID));
            }
        } else if (TargetAction == FeatureAction::Deactivate) {
            uint16_t ActiveSubStreamID = Instance.GetFirstSubStreamForFeature(FeatureIndex);
            if (ActiveSubStreamID != 0) {
                ToggleData.SubStreamID = ActiveSubStreamID;
            }
            CloseSubStreams(DeviceID, FeatureIndex);
        }
    }

    OmniNetCommand ToggleCmd{
        CoreCommandsWArgs::ToggleFeature,
        Variance::GetVariantTypeIndex<FeatureToggleData, FuncArgTypes>,
        FeatureToggleData::Serialize(ToggleData)
    };
    TransmitNetCommand(DeviceID, ToggleCmd, 0, OmniNet::Argonized);

    UpdateFeatureState(DeviceID, FeatureIndex, FeatureActionRoute::Outbound, TargetAction);
}

void OmniCore::FeatureStateHandler(DeviceMap DeviceID, const FeatureToggleData& FeatureData)
{
    OmniNet::PoolConfig PoolConfig = UpdateFeatureState(
        DeviceID, FeatureData.FeatureType, FeatureActionRoute::Inbound, FeatureData.Action
    );

    if (FeatureData.Action == FeatureAction::Deactivate && FeatureData.SubStreamID != 0) {
        CloseSubStream(DeviceID, FeatureData.SubStreamID);
        return;
    }

    if (FeatureData.Action == FeatureAction::Activate && FeatureData.SubStreamID != 0) {
        if (!InstanceRegistry.ActiveInstances.contains(DeviceID))
            return;

        auto& Instance = InstanceRegistry.ActiveInstances.at(DeviceID);
        SubStreamEntry* Entry = Instance.FindSubStream(FeatureData.SubStreamID);

        if (!Entry || !Entry->SubStream) {
            Logger::log(
                "uh.. SubStreamID={:d} not found for device {:d} — "
                "SubStream Creation Request may not have arrived yet",
                FeatureData.SubStreamID,
                static_cast<int>(DeviceID)
            );
            return;
        }

        Instance.RegisterFeatureSubStream(FeatureData.FeatureType, FeatureData.SubStreamID);

        if (PoolConfig.Data != nullptr) {
            ConfigureSubStream(DeviceID, FeatureData.SubStreamID, PoolConfig);
        }
    }
}

OmniNetSubStream* OmniCore::OpenSubStream(DeviceMap Device, uint16_t SubStreamID)
{
    if (!InstanceRegistry.ActiveInstances.contains(Device)) {
        Logger::log(
            "SubStream for Non-Existent Device {:d}? How did we get here ?",
            static_cast<int>(Device)
        );
        return nullptr;
    }

    auto& Instance = InstanceRegistry.ActiveInstances.at(Device);

    OmniNetSubStream* SubStream = Instance.InstanceSession->OpenSubStream();
    if (!SubStream) {
        Logger::log(
            "No free SubStream slots for device {:d}, hopefully that's the case",
            static_cast<int>(Device)
        );
        return nullptr;
    }

    Instance.SubStreamRegistry[SubStreamID] = SubStreamEntry{SubStream, SubStreamState::Pending};

    Logger::log(
        "SubStream Awaiting @SubStreamID={:d} Port={:d} for device {:d}",
        SubStreamID,
        SubStream->GetLocalPort(),
        static_cast<int>(Device)
    );

    return SubStream;
}

void OmniCore::ConfigureSubStream(
    DeviceMap Device, uint16_t SubStreamID, const OmniNet::PoolConfig& Config
)
{
    if (!InstanceRegistry.ActiveInstances.contains(Device)) {
        Logger::log(
            "SubStream for Non-Existent Device {:d}? How did we get here ?",
            static_cast<int>(Device)
        );
        return;
    }

    auto& Instance = InstanceRegistry.ActiveInstances.at(Device);

    SubStreamEntry* Entry = Instance.FindSubStream(SubStreamID);
    if (!Entry || !Entry->SubStream) {
        Logger::log(
            "Can't Configure A Non-Existent SubStreamID={:d} Linked To Device {:d}",
            SubStreamID,
            static_cast<int>(Device)
        );
        return;
    }

    if (!Entry->SubStream->BindRecvPool(
            Config.Data, Config.DataSize, Config.NumSlots, Config.OnSlotComplete, Config.Ctx
        )) {
        Logger::log(
            "BindRecvPool failed for SubStreamID={:d} on Device {:d}",
            SubStreamID,
            static_cast<int>(Device)
        );
        return;
    }

    Logger::log(
        "Recv pool bound for SubStreamID={:d} on Device {:d}", SubStreamID, static_cast<int>(Device)
    );
}

void OmniCore::SubStreamHandler(DeviceMap Device, SubStreamData Data)
{
    if (!InstanceRegistry.ActiveInstances.contains(Device)) {
        Logger::log(
            "SubStream for Non-Existent Device {:d}? How did we get here ?",
            static_cast<int>(Device)
        );
        return;
    }

    auto& Instance = InstanceRegistry.ActiveInstances.at(Device);

    switch (Data.Action) {
    case SubStreamAction::Create: {
        OmniNetSubStream* SubStream = Instance.InstanceSession->OpenSubStream();
        if (!SubStream) {
            Logger::log(
                "No free SubStream slots for device {:d}, hopefully that's the case",
                static_cast<int>(Device)
            );
            return;
        }

        Instance.SubStreamRegistry[Data.SubStreamID] =
            SubStreamEntry{SubStream, SubStreamState::Pending};

        if (Data.Port) {
            if (!SubStream->Connect(Instance.IPv4_String, Data.Port)) {
                Logger::log(
                    "SubStream Connection to {:s}:{:d} failed", Instance.IPv4_String, Data.Port
                );
                Instance.SubStreamRegistry.erase(Data.SubStreamID);
                Instance.InstanceSession->CloseSubStream(SubStream);
                return;
            }

            Instance.SubStreamRegistry[Data.SubStreamID].State = SubStreamState::Active;

            SubStreamData StreamConfig{
                SubStreamAction::Connect, Data.SubStreamID, SubStream->GetLocalPort()
            };
            OmniNetCommand NetCommand{
                CoreCommandsWArgs::SubStream,
                Variance::GetVariantTypeIndex<SubStreamData, FuncArgTypes>,
                SubStreamData::Serialize(StreamConfig)
            };

            TransmitNetCommand(Device, NetCommand, 0, OmniNet::Argonized);

            Logger::log(
                "SubStream Active @SubStreamID={:d} connected to ({:s}:{:d}), Port={:d}",
                Data.SubStreamID,
                Instance.IPv4_String,
                Data.Port,
                SubStream->GetLocalPort()
            );
        }
        break;
    }

    case SubStreamAction::Connect: {
        SubStreamEntry* Entry = Instance.FindSubStream(Data.SubStreamID);
        if (!Entry || Entry->State != SubStreamState::Pending) {
            Logger::log(
                "SubStream @SubStreamID={:d} not Awaiting for device {:d}",
                Data.SubStreamID,
                static_cast<int>(Device)
            );
            return;
        }

        if (!Entry->SubStream->Connect(Instance.IPv4_String, Data.Port)) {
            Logger::log(
                "SubStream Connection To {:s}:{:d} Failed", Instance.IPv4_String, Data.Port
            );
            return;
        }

        Entry->State = SubStreamState::Active;

        Logger::log("SubStream on SubStreamID={:d} Fully Connected", Data.SubStreamID);
        break;
    }

    case SubStreamAction::Disconnect: {
        CloseSubStream(Device, Data.SubStreamID, false);
        break;
    }

    default:
        break;
    }
}

void OmniCore::CloseSubStream(DeviceMap DeviceID, uint16_t SubStreamID, bool NotifyPeer)
{
    if (!InstanceRegistry.ActiveInstances.contains(DeviceID))
        return;

    auto& Instance = InstanceRegistry.ActiveInstances.at(DeviceID);

    SubStreamEntry* Entry = Instance.FindSubStream(SubStreamID);
    if (!Entry) {
        Logger::log(
            "Can't cleanup non-existent SubStreamID={:d} for device {:d}",
            SubStreamID,
            static_cast<int>(DeviceID)
        );
        return;
    }

    Entry->State = SubStreamState::Terminating;

    if (NotifyPeer) {
        SubStreamData DisconnectData{SubStreamAction::Disconnect, SubStreamID, 0};
        OmniNetCommand DisconnectCmd{
            CoreCommandsWArgs::SubStream,
            Variance::GetVariantTypeIndex<SubStreamData, FuncArgTypes>,
            SubStreamData::Serialize(DisconnectData)
        };
        TransmitNetCommand(DeviceID, DisconnectCmd, 0, OmniNet::Argonized);
    }

    if (Entry->SubStream) {
        Instance.InstanceSession->CloseSubStream(Entry->SubStream);
        Entry->SubStream = nullptr;
    }

    Instance.UnregisterFeatureSubStream(SubStreamID);
    Instance.SubStreamRegistry.erase(SubStreamID);

    Logger::log(
        "SubStream SubStreamID={:d} For Device {:d} Exterminated !",
        SubStreamID,
        static_cast<int>(DeviceID)
    );
}

void OmniCore::CloseSubStreams(DeviceMap DeviceID, FeatureTypes Feature)
{
    if (!InstanceRegistry.ActiveInstances.contains(DeviceID))
        return;

    auto& Instance = InstanceRegistry.ActiveInstances.at(DeviceID);

    std::vector<uint16_t> SubStreamIDs;
    auto range = Instance.FeatureSubStreams.equal_range(Feature);
    for (auto it = range.first; it != range.second; ++it) {
        SubStreamIDs.push_back(it->second);
    }

    for (uint16_t ID : SubStreamIDs) {
        CloseSubStream(DeviceID, ID);
    }
}
