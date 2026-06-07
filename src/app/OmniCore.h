#ifndef OMNICORE_H
#define OMNICORE_H

#pragma once
#include "resource.h"

#include "BurstQ.h"
#include "CaptureController.h"
#include "D3D11Renderer.h"
#include "IOLink.h"
#include "InstanceRegistry.h"
#include "OmniEnums.h"
#include "OmniLogger.h"
#include "OmniPackets.h"
#include "OmniTypes.h"
#include "SessionHandler.h"
#include "SessionManager.h"
#include "nvenc.h"

#include <unordered_map>
#include <vector>

#include <Windows.h>
#include <array>
#include <dwmapi.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dcomp.h>
#include <directxmath.h>
#include <dxgi1_5.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dcomp.lib")
#include <comdef.h>
#include <wincodec.h>

#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")

#define WM_TRAYICON (WM_USER + 1)

class OmniCore
{
  protected:
    AppState OmniState = AppState::RUNNING;
    GUIState OmniGUIState = GUIState::RENDER;

    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    D3DState OmniRenderState;
    InstanceRegistry InstanceReg;
    SessionManager SessionMgr;
    CaptureController CaptureCtrl;

    OmniCap OmniCap{InstanceReg.ActiveInstances};
    OmniSynth OmniSynth;

    NVENCODER* NVENC = nullptr;

    DeviceMap SelectedInstance = DeviceMap::L1;
    std::vector<WinForge*> ActiveWindows{};

    // Active Input Proc Target Device
    static DeviceMap ActiveIOProcTarget;
    static DeviceMap SelectedTargetDevice;

    OmniShield InputFilter;

  public:
    OmniCore();

    inline void OmniCmdStatus() { Logger::log("CMD Queue Status Test"); }

    inline std::unordered_map<DeviceMap, OmniInstance>* GetAvailableInstances()
    {
        return &InstanceReg.AllInstances;
    }

    // Core Functions

    void ScanInstances();

    void ConnectInstance(DeviceMap DeviceID);

    void SwapInstanceLayout(int DeviceID1, int DeviceID2);

    void CreateStreamLink(WindowCreationData& WindowInfo);

    // Command Queue System

    std::mutex CommandQMutex;
    std::condition_variable CommandQCV;
    std::thread CommandQThread;

    std::array<void (OmniCore::*)(), 10> CommandTable = {&OmniCore::OmniCmdStatus,
                                                         &OmniCore::ScanInstances};

    // This defines the maximum number of commands that can be queued using
    // PushCommand functions and drained with ExecuteCommandQueue.
    BurstQ<CoreCommands, 16> CommandBurstQ = BurstQ<CoreCommands, 16>();

    /// This defines the maximum number of commands with args that can be queued, memory taken up by
    /// the queue will be based on the biggest size argument structure defined in FuncArgTypes.
    BurstQ<FuncArgTypes, 16> CommandBurstQWArgs = BurstQ<FuncArgTypes, 16>();

    inline void RunCommandQueue()
    {
        if (!CommandQThread.joinable()) {
            CommandQThread = std::thread([this]() -> void {
                while (OmniState == AppState::RUNNING) {
                    std::unique_lock<std::mutex> lock(CommandQMutex);
                    CommandQCV.wait(lock, [this]() -> boolean {
                        return !CommandBurstQ.Queue.empty() || !CommandBurstQWArgs.Queue.empty();
                    });

                    ExecuteCommandQueue();
                    ExecuteCommandQueueWArgs();
                }
            });
        }
    }

    inline void StopCommandQueue()
    {
        {
            std::unique_lock<std::mutex> lock(CommandQMutex);
            OmniState = AppState::STOPPING;
        }

        CommandQCV.notify_all();
        if (CommandQThread.joinable()) {
            CommandQThread.join();
        }
    }

    inline void NotifyCommandQueue() { CommandQCV.notify_one(); }

    inline void ExecuteCommandQueue()
    {
        while (!CommandBurstQ.Queue.empty()) {
            (this->*CommandTable[CommandBurstQ.Queue[CommandBurstQ.Tail]])();
            CommandBurstQ.pop();
        }
    }

    inline void ExecuteCommandQueueWArgs()
    {
        while (!CommandBurstQWArgs.Queue.empty()) {
            unsigned int Tail = CommandBurstQWArgs.Tail;
            switch (CommandBurstQWArgs.Queue[Tail].index()) {
            case 0: {
                ArraySwapLayout& args = std::get<0>(CommandBurstQWArgs.Queue[Tail]);
                (this->SwapInstanceLayout)(args.index1, args.index2);
                CommandBurstQWArgs.pop();
                break;
            }

            case 1: {
                ConnectionRequest args = std::get<1>(CommandBurstQWArgs.Queue[Tail]);
                (this->ConnectInstance)(args.DeviceID);
                CommandBurstQWArgs.pop();
                break;
            }

            case 2: {
                WindowCreationData args = std::get<2>(CommandBurstQWArgs.Queue[Tail]);
                (this->CreateStreamLink)(args);
                CommandBurstQWArgs.pop();
                break;
            }
            }
        }
    }

    inline void PushCommand(CoreCommands CommandType) { CommandBurstQ.push(CommandType); }

    inline void PushCommands(std::vector<CoreCommands>& CommandTypeArray)
    {
        for (CoreCommands command : CommandTypeArray) {
            CommandBurstQ.push(command);
        }
    }

    inline void PushCommandWArgs(FuncArgTypes& CommandArgs)
    {
        CommandBurstQWArgs.push(CommandArgs);
    }

    inline void TransmitNetCommand(DeviceMap TargetDevice,
                                   OmniNetCommand& Command,
                                   uint8_t Target = 0,
                                   uint8_t Flags = 0)
    {
        OmniNet::OmniHeader header;
        header.PacketType = OmniNet::PacketType::Command;
        header.Target = Target;
        header.Flags = Flags;

        std::vector<uint8_t> payload = OmniNetCommand::Serialize(Command);

        InstanceReg.ActiveInstances[TargetDevice].InstanceSession->SessionSend(
            reinterpret_cast<char*>(payload.data()), payload.size(), header);
    }

    void ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index);
};

#endif
