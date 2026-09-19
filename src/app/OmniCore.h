#ifndef OMNICORE_H
#define OMNICORE_H

#pragma once

#include "BurstQ.hpp"
#include "InstanceRegistry.hpp"
#include "OmniDiscovery.hpp"
#include "OmniEnums.hpp"
#include "OmniGraphicsContext.hpp"
#include "OmniLogger.hpp"
#include "OmniPackets.hpp"
#include "OmniQrypt.hpp"
#include "OmniTypes.hpp"
#include "SessionHandler.hpp"
#include "SessionManager.hpp"
#include "SystemLink.hpp"
#include "UIEvents.hpp"
#include "nvenc.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class OmniCore
{
  protected:
    OmniAppState AppState = OmniAppState::RUNNING;
    OmniGUIState UIState  = OmniGUIState::RENDER;

    const float          ClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    OmniGraphicsContext  GraphicsContext;
    OmniInstanceRegistry InstanceRegistry;
    OmniSessionManager   SessionManager;
    OmniQrypt            QryptManager;

    NVENCODER* NVENC = nullptr;

    DeviceMap SelectedInstance = DeviceMap::L1;

    OmniSystemLink SystemLink{GraphicsContext};

    static DeviceMap ActiveIOProcTarget;

    struct HandshakeRetryContext
    {
        DeviceMap                             DeviceID;
        uint32_t                              Token;
        int                                   RetriesLeft;
        std::chrono::steady_clock::time_point NextRetry;
        bool                                  Active = false;
    };

    std::unordered_map<DeviceMap, HandshakeRetryContext> HandshakeRetries;

    static uint32_t GenerateHandshakeToken();

    void HandleHandshakeRetries();

    void FailHandshake(DeviceMap DeviceID, const char* Reason);

  public:
    static DeviceMap SelectedTargetDevice;

    OmniCore();

    virtual void PushNotification(const Notification& notification) {}
    virtual void PushNotification(DeviceMap DeviceID, const Notification& notification) {}
    virtual void CancelNotification(DeviceMap DeviceID) {}

    virtual void DragWindow() {}
    virtual void MinimizeWindow() {}
    virtual void HideWindow() {}

    inline std::unordered_map<DeviceMap, OmniInstance>* GetAvailableInstances()
    {
        return &InstanceRegistry.AllInstances;
    }

    inline int GetAvailableDeviceCount() { return InstanceRegistry.GetAllInstancesCount(); }

    inline ActiveInstanceContainer* GetActiveInstances()
    {
        return &InstanceRegistry.ActiveInstances;
    }

    const std::vector<OmniInstanceGroup>& GetInstanceGroups() const
    {
        return InstanceRegistry.GetInstanceGroups();
    }

    void
    SaveCurrentGroup(const char* Name = "Preset Group", const char* Subtitle = "Nothing Special")
    {
        InstanceRegistry.SaveCurrentGroup(Name, Subtitle);
    }

    void ConnectGroup(size_t Index);

    void RemoveInstanceGroup(size_t Index) { InstanceRegistry.RemoveInstanceGroup(Index); }

    void AddManualInstance(uint32_t IP, DeviceMap DeviceID = DeviceMap::END)
    {
        InstanceRegistry.AddInstance(IP, DeviceID);
    }

    OmniQrypt* GetQryptManager() { return &QryptManager; }

    const OmniQrypt* GetQryptManager() const { return &QryptManager; }

    void ResetInstance(DeviceMap DeviceID);

    void ForgetDevice(DeviceMap deviceID)
    {
        ResetInstance(deviceID);
        QryptManager.ClearSession(deviceID);
    }

    inline void OmniCmdStatus() { Logger::log("CMD Queue Status Test"); }

    void DiscoveryPacketHandler(ProbeEvent Event);

    void ScanInstances();

    void RequestHandshake(DeviceMap DeviceID);

    void HandshakeHandler(HandshakeData Data);

    void AcceptConnection(DeviceMap DeviceID, bool trustPermanently);

    void RejectConnection(DeviceMap DeviceID);

    void ConnectInstance(DeviceMap DeviceID);

    void SwapInstanceLayout(int DeviceID1, int DeviceID2);

    std::mutex              CommandQMutex;
    std::condition_variable CommandQCV;
    std::thread             CommandQThread;

    std::array<void (OmniCore::*)(), 10> CommandTable = {
        &OmniCore::OmniCmdStatus, &OmniCore::ScanInstances
    };

    BurstQ<CoreCommands, 16> CommandBurstQ = BurstQ<CoreCommands, 16>();

    struct CommandQItem
    {
        DeviceMap    DeviceID = DeviceMap::C0;
        FuncArgTypes Args     = ArraySwapLayout{0, 0};
    };

    BurstQ<CommandQItem, 16> CommandBurstQWArgs = BurstQ<CommandQItem, 16>();

    inline void RunCommandQueue()
    {
        if (!CommandQThread.joinable()) {
            CommandQThread = std::thread([this]() -> void {
                while (AppState == OmniAppState::RUNNING) {
                    std::unique_lock<std::mutex> lock(CommandQMutex);

                    if (!HandshakeRetries.empty())
                        CommandQCV.wait_for(lock, std::chrono::milliseconds(500), [this]() -> bool {
                            return !CommandBurstQ.empty() || !CommandBurstQWArgs.empty() ||
                                   AppState != OmniAppState::RUNNING;
                        });
                    else
                        CommandQCV.wait(lock, [this]() -> bool {
                            return !CommandBurstQ.empty() || !CommandBurstQWArgs.empty() ||
                                   AppState != OmniAppState::RUNNING;
                        });

                    ExecuteCommandQueue();
                    ExecuteCommandQueueWArgs();
                    HandleHandshakeRetries();
                }
            });
        }
    }

    inline void StopCommandQueue()
    {
        {
            std::unique_lock<std::mutex> lock(CommandQMutex);
            AppState = OmniAppState::STOPPING;
        }

        CommandQCV.notify_all();
        if (CommandQThread.joinable()) {
            CommandQThread.join();
        }
    }

    inline void NotifyCommandQueue() { CommandQCV.notify_one(); }

    inline void ExecuteCommandQueue()
    {
        while (!CommandBurstQ.empty()) {
            (this->*CommandTable[CommandBurstQ.Queue[CommandBurstQ.Tail]])();
            if (!CommandBurstQ.pop()) {
                Logger::log("Command Execution Failure");
            }
        }
    }

    inline void ExecuteCommandQueueWArgs()
    {
        while (!CommandBurstQWArgs.empty()) {
            unsigned int  Tail     = CommandBurstQWArgs.Tail;
            CommandQItem& Command  = CommandBurstQWArgs.Queue[Tail];
            DeviceMap     DeviceID = Command.DeviceID;

            switch (Command.Args.index()) {
            case 0: {
                ArraySwapLayout& args = std::get<0>(Command.Args);
                (SwapInstanceLayout)(args.index1, args.index2);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }

                break;
            }

            case 1: {
                ConnectionRequest args = std::get<1>(Command.Args);
                (ConnectInstance)(args.DeviceID);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }
                break;
            }

            case 2: {
                HandshakeData args = std::get<2>(Command.Args);
                HandshakeHandler(args);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }

                break;
            }
            case 3: {
                HandshakeResponse args = std::get<3>(Command.Args);
                if (args.State == HandshakeResponse::Action::ACCEPT) {
                    AcceptConnection(args.DeviceID, args.Trusted);
                } else {
                    RejectConnection(args.DeviceID);
                }

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }

                break;
            }
            case 4: {
                FeatureToggleData args = std::get<4>(Command.Args);
                FeatureStateHandler(DeviceID, args);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }

                break;
            }
            case 5: {
                SubStreamData args = std::get<5>(Command.Args);
                SubStreamHandler(DeviceID, args);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }

                break;
            }
            }
        }
    }

    inline void PushCommand(CoreCommands CommandType)
    {
        if (!CommandBurstQ.push(CommandType)) {
            Logger::log("Failed To Queue Command");
        }
    }

    inline void PushCommands(std::vector<CoreCommands>& CommandTypeArray)
    {
        for (CoreCommands command : CommandTypeArray) {
            if (!CommandBurstQ.push(command)) {
                Logger::log("Failed To Queue Commands");
            }
        }
    }

    inline void PushCommandWArgs(DeviceMap DeviceID, FuncArgTypes& CommandArgs)
    {
        if (!CommandBurstQWArgs.push(CommandQItem{DeviceID, CommandArgs})) {
            Logger::log("Failed To Queue Commands With Args");
        }
    }

    inline void TransmitNetCommand(
        DeviceMap TargetDevice, OmniNetCommand& Command, uint8_t Target = 0, uint8_t Flags = 0
    )
    {
        if (Command.CommandType >= CoreCommandsWArgs::AuthlessGate && Command.ActionToken == 0) {
            Command.ActionToken = QryptManager.CreateActionToken(
                TargetDevice,
                static_cast<uint8_t>(Command.CommandType),
                static_cast<uint64_t>(Command.ArgTypeIndex)
            );

            if (Command.ActionToken == 0) {
                Logger::log(
                    "Failed to generate ActionToken for command {:d} targeting device {:d}, "
                    "transmission aborted!",
                    static_cast<int>(Command.CommandType),
                    static_cast<int>(TargetDevice)
                );
                return;
            }
        }

        OmniNet::OmniHeader header;
        header.PacketType = OmniNet::PacketType::Command;
        header.Target     = Target;
        header.Flags      = Flags;

        std::vector<uint8_t> payload = OmniNetCommand::Serialize(Command);

        if (InstanceRegistry.ActiveInstances.contains(TargetDevice)) {
            InstanceRegistry.ActiveInstances.at(TargetDevice)
                .InstanceSession->SessionSend(
                    reinterpret_cast<char*>(payload.data()), payload.size(), header
                );
        }
    }

    // Feature toggling system for user actions
    // Notes for anyone wondering:
    // WindowLink and ScreenLink are mutually exclusive per device.
    // If ScreenLink is active, the entire desktop is already streamed, so WindowLink is redundant.
    // InputLink doesn't need a UDP SubStream while ClipboardLink spins up TCP streams on the go.
    // ScreenLink and AudioLink both just need 1 UDP SubStream per device hence grouped.
    // WindowLink needs UDP SubStreams on the go unlike ScreenLink and AudioLink that uses only one.
    // Additionally WindowLink needs Input Link to function, otherwise u can't drag a window can u..
    void ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index, void* Context = nullptr);
    void FeatureStateHandler(DeviceMap SenderID, const FeatureToggleData& ToggleConfig);

    OmniNet::PoolConfig UpdateFeatureState(
        DeviceMap          Device,
        FeatureTypes       Feature,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    OmniNet::PoolConfig DispatchFeatureState(
        FeatureTypes       Feature,
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );

    // Sub stream management
    // Opens a sub stream, requests the other device to create one as well.
    // Registers the sub stream info
    SubStreamID OpenSubStream(DeviceMap DeviceID, FeatureTypes Feature);

    // Used for late binding receive pools onto a substream
    void ConfigureSubStream(
        DeviceMap Device, SubStreamID SubStreamID, const OmniNet::PoolConfig& Config
    );

    // Closes a sub stream and sends a disconnection request to the peer if needed
    void CloseSubStream(DeviceMap DeviceID, SubStreamID SubStreamID, bool NotifyPeer = true);

    void CloseSubStreams(DeviceMap DeviceID, FeatureTypes Feature);

    void SubStreamHandler(DeviceMap Device, SubStreamData Data);

    // Action Token Verification
    inline bool VerifyCommandToken(DeviceMap DeviceID, const OmniNetCommand& Command) const
    {
        if (Command.CommandType < CoreCommandsWArgs::AuthlessGate) {
            return true;
        }

        return QryptManager.VerifyActionToken(
            DeviceID,
            static_cast<uint8_t>(Command.CommandType),
            static_cast<uint64_t>(Command.ArgTypeIndex),
            Command.ActionToken
        );
    }

    inline bool VerifyCommandToken(const OmniNetCommand& Command) const
    {
        if (Command.CommandType < CoreCommandsWArgs::AuthlessGate) {
            return true;
        }

        for (const auto& [DeviceID, Instance] : InstanceRegistry.ActiveInstances) {
            if (QryptManager.VerifyActionToken(
                    DeviceID,
                    static_cast<uint8_t>(Command.CommandType),
                    static_cast<uint64_t>(Command.ArgTypeIndex),
                    Command.ActionToken
                )) {
                return true;
            }
        }
        return false;
    }
};

#endif
