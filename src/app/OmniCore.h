#ifndef OMNICORE_H
#define OMNICORE_H

#pragma once

#include "BurstQ.h"
#include "InstanceRegistry.h"
#include "OmniDiscovery.h"
#include "OmniEnums.h"
#include "OmniLogger.h"
#include "OmniPackets.h"
#include "OmniQrypt.h"
#include "OmniTypes.h"
#include "RenderState.h"
#include "SessionHandler.h"
#include "SessionManager.h"
#include "SystemLink.h"
#include "UIEvents.h"
#include "nvenc.h"

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
    OmniGUIState UIState = OmniGUIState::RENDER;

    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    OmniRenderState RenderState;
    OmniInstanceRegistry InstanceRegistry;
    OmniSessionManager SessionManager;
    OmniQrypt QryptManager;

    NVENCODER* NVENC = nullptr;

    DeviceMap SelectedInstance = DeviceMap::L1;

    OmniSystemLink SystemLink{RenderState};

    static DeviceMap ActiveIOProcTarget;

    struct HandshakeRetryContext
    {
        DeviceMap DeviceID;
        uint32_t Token;
        int RetriesLeft;
        std::chrono::steady_clock::time_point NextRetry;
        bool Active = false;
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

    inline void OmniCmdStatus() { Logger::log("CMD Queue Status Test"); }

    void DiscoveryPacketHandler(ProbeEvent Event);

    void ScanInstances();

    void RequestHandshake(DeviceMap DeviceID);

    void HandshakeHandler(HandshakeData Data);

    void AcceptConnection(DeviceMap DeviceID, bool trustPermanently);

    void RejectConnection(DeviceMap DeviceID);

    void ConnectInstance(DeviceMap DeviceID);

    void SwapInstanceLayout(int DeviceID1, int DeviceID2);

    void CreateStreamLink(WindowCreationData& WindowInfo);

    std::mutex CommandQMutex;
    std::condition_variable CommandQCV;
    std::thread CommandQThread;

    std::array<void (OmniCore::*)(), 10> CommandTable = {
        &OmniCore::OmniCmdStatus, &OmniCore::ScanInstances
    };

    BurstQ<CoreCommands, 16> CommandBurstQ = BurstQ<CoreCommands, 16>();
    BurstQ<FuncArgTypes, 16> CommandBurstQWArgs = BurstQ<FuncArgTypes, 16>();

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
            unsigned int Tail = CommandBurstQWArgs.Tail;
            switch (CommandBurstQWArgs.Queue[Tail].index()) {
            case 0: {
                ArraySwapLayout& args = std::get<0>(CommandBurstQWArgs.Queue[Tail]);
                (SwapInstanceLayout)(args.index1, args.index2);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }

                break;
            }

            case 1: {
                ConnectionRequest args = std::get<1>(CommandBurstQWArgs.Queue[Tail]);
                (ConnectInstance)(args.DeviceID);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }
                break;
            }

            case 2: {
                WindowCreationData args = std::get<2>(CommandBurstQWArgs.Queue[Tail]);
                (CreateStreamLink)(args);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }
                break;
            }
            case 3: {
                HandshakeData args = std::get<3>(CommandBurstQWArgs.Queue[Tail]);
                HandshakeHandler(args);

                if (!CommandBurstQWArgs.pop()) {
                    Logger::log("Command Execution Failure");
                }

                break;
            }
            case 4: {
                HandshakeResponse args = std::get<4>(CommandBurstQWArgs.Queue[Tail]);
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

    inline void PushCommandWArgs(FuncArgTypes& CommandArgs)
    {
        if (!CommandBurstQWArgs.push(CommandArgs)) {
            Logger::log("Failed To Queue Commands With Args");
        }
    }

    inline void TransmitNetCommand(
        DeviceMap TargetDevice, OmniNetCommand& Command, uint8_t Target = 0, uint8_t Flags = 0
    )
    {
        OmniNet::OmniHeader header;
        header.PacketType = OmniNet::PacketType::Command;
        header.Target = Target;
        header.Flags = Flags;

        std::vector<uint8_t> payload = OmniNetCommand::Serialize(Command);

        if (InstanceRegistry.ActiveInstances.contains(TargetDevice)) {
            InstanceRegistry.ActiveInstances.at(TargetDevice)
                .InstanceSession->SessionSend(
                    reinterpret_cast<char*>(payload.data()), payload.size(), header
                );
        }
    }

    void ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index);
};

#endif
