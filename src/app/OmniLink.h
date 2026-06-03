#ifndef UNICODE
#define UNICODE
#endif

#ifndef OMNILINK_H
#define OMNILINK_H

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "resource.h"

#include "AsyncWorker.h"
#include "BurstQ.h"
#include "IOLink.h"
#include "InstanceRegistry.h"
#include "OmniAPI.h"
#include "OmniConfig.h"
#include "OmniEnums.h"
#include "OmniGUI.h"
#include "OmniLogger.h"
#include "OmniPackets.h"
#include "OmniRenderer.h"
#include "OmniTypes.h"
#include "SessionHandler.h"
#include "SessionManager.h"
#include "WinCap.h"
#include "nvenc.h"
#include "platform/CaptureController.h"

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

#include <lz4.h>

#ifndef OMNI_BUILD_RELEASE
#pragma comment(lib, "lz4d.lib")
#endif

#ifndef OMNI_BUILD_DEBUG
#pragma comment(lib, "lz4.lib")
#endif

#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")

#define WM_TRAYICON (WM_USER + 1)

class OmniCore
{
  protected:
    HINSTANCE hInstance;
    int nCmdShow;

    HANDLE* Events = nullptr;
    DWORD EventDW = NULL;

    D3DState OmniRenderState;
    InstanceRegistry InstanceReg;
    SessionManager SessionMgr;
    CaptureController CaptureCtrl;

    OmniCap OmniCap{InstanceReg.ActiveInstances};
    OmniSynth OmniSynth;

    NVENCODER* NVENC = nullptr;

    DeviceMap SelectedInstance = DeviceMap::L1;
    std::vector<WinForge*> ActiveWindows{};

  public:
    OmniCore(HINSTANCE hInstance, int nCmdShow) : hInstance(hInstance), nCmdShow(nCmdShow) {};

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
    std::array<void (OmniCore::*)(), 10> CommandTable = {&OmniCore::OmniCmdStatus,
                                                         &OmniCore::ScanInstances};

    // This defines the maximum number of commands that can be queued using
    // PushCommand functions and drained with ExecuteCommandQueue.
    BurstQ<CoreCommands, 20> CommandBurstQ = BurstQ<CoreCommands, 20>();

    /// This defines the maximum number of commands with args that can be queued, memory taken up by
    /// the queue will be based on the biggest size argument structure defined in FuncArgTypes.
    BurstQ<FuncArgTypes, 20> CommandBurstQWArgs = BurstQ<FuncArgTypes, 20>();

    inline void ExecuteCommandQueue() { SetEvent(Events[4]); }

    inline void ExecuteCommandQueueWArgs() { SetEvent(Events[5]); }

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
};

class OmniLink : public OmniCore
{
  private:
    OmniGUI* GUI = nullptr;

    HWND hwnd = 0;

    NOTIFYICONDATAW TrayIconData = {};

    std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(15 * 1000000);

    std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
        std::chrono::steady_clock::now();

    float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    void (OmniLink::*ExecuteCommand)() = &OmniLink::CommandListEmpty;

    static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Active Input Proc Target Device
    static DeviceMap ActiveIOProcTarget;
    static DeviceMap SelectedTargetDevice;

    MSG msg = {};

    OmniShield InputFilter;

    NvencSession* NvencSessionPtr = nullptr;

    AsyncWorker::Uncached AsynLink;

    void OmniMainLoop();

    void InitTrayIcon(HWND hwnd);

  public:
    OmniLink(HINSTANCE hInst, int nCmdShow) : OmniCore(hInst, nCmdShow) {};

    void OmniMain(HINSTANCE hInstance, int nCmdShow);

    void ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index);

    inline void CommandListEmpty() {}
};

#endif
