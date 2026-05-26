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

#include "ByteStream.h"
#include "IOLink.h"
#include "OmniAPI.h"
#include "OmniDiscovery.h"
#include "OmniGUI.h"
#include "OmniLogger.h"
#include "OmniTypes.h"
#include "SessionHandler.h"
#include "WinCap.h"
#include "WinForge.h"
#include "nvenc.h"

#include <Windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#include <array>
#include <shellapi.h>
#include <variant>
#include <vector>
#include <wrl/client.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dcomp.h>
#include <directxmath.h>
#include <dxgi1_5.h>

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

#define OmniPort 62485
#define MTU 1450

class OmniCore {

public:
  OmniCore(HINSTANCE hInst, int nCmdShow);

  inline void OmniCmdStatus() { Logger::log("CMD Queue Status Test\n"); }

  // Core Functions

  void WinGetUserName(char (&CharArray)[UNLEN + 1]);

  void WinGetComputerName(char (&CharArray)[OmniDevNameLen + 1]);

  void QueryLocalIP(uint32_t &LocalIP, const int index = 0);

  void ScanInstances();

  void Connect(char IP[16], char Auth[4]);

  void ConnectInstance(ConnectionRequest request);

  void SwapInstanceLayout(int index1, int index2);

  void CreateStreamLink(WindowCreationData &WindowInfo);

  std::unordered_map<DeviceMap, OmniInstance> *GetAvailableInstances() noexcept;

  // Command Queue System
  std::array<void (OmniCore::*)(), 10> CommandTable = {&OmniCore::OmniCmdStatus,
                                                       &OmniCore::ScanInstances

  };

  /// <summary>
  /// This defines the maximum number of commands that can be queued using
  /// PushCommand functions and drained with ExecuteCommandQueue.
  /// </summary>
  BurstQ<CoreCommands, 20> CommandBurstQ = BurstQ<CoreCommands, 20>();

  /// <summary>
  /// Same as above but for commands with args, memory taken up by the queue
  /// will be based on the biggest size argument structure defined in
  /// FuncArgTypes.
  /// </summary>
  BurstQ<FuncArgTypes, 20> CommandBurstQWArgs = BurstQ<FuncArgTypes, 20>();

  inline void ExecuteCommandQueue() { SetEvent(Events[4]); }

  inline void ExecuteCommandQueueWArgs() { SetEvent(Events[5]); }

  inline void PushCommand(CoreCommands CommandType) {
    CommandBurstQ.push(CommandType);
  }

  inline void PushCommands(std::vector<CoreCommands> &CommandTypeArray) {
    for (CoreCommands command : CommandTypeArray) {
      CommandBurstQ.push(command);
    }
  }

  inline void PushCommandWArgs(FuncArgTypes &CommandArgs) {
    CommandBurstQWArgs.push(CommandArgs);
  }

  inline void TransmitNetCommand(DeviceMap TargetDevice,
                                 OmniNetCommand &Command, uint8_t Target = 0,
                                 uint8_t Flags = 0) {
    OmniNet::OmniHeader header;
    header.PacketType = OmniNet::PacketType::Command;
    header.Target = Target;
    header.Flags = Flags;

    std::vector<uint8_t> payload = OmniNetCommand::Serialize(Command);

    ActiveInstances[TargetDevice].InstanceSession->SessionSend(
        reinterpret_cast<char *>(payload.data()), payload.size(), header);
  }

  template <typename Variant, std::size_t... SequenceIndex>
  inline void static NetVariantDeserializer(
      Variant &Dest, size_t Index, std::index_sequence<SequenceIndex...>,
      uint8_t *Buffer, const uint32_t BufferLen) {
    ByteStreamReader Reader{BufferLen, Buffer};
    ((Index == SequenceIndex &&
      (Dest = std::variant_alternative_t<SequenceIndex, Variant>::Deserialize(
           Reader),
       true)) ||
     ...);
  }

  /*template<typename Variant, std::size_t... SequenceIndex>
  inline void static NetVariantSerializer(Variant& Dest, size_t Index,
  std::index_sequence<SequenceIndex ...>, const uint8_t* Buffer, const uint32_t
  BufferLen)
  {
          ByteStreamReader Reader{ BufferLen, Buffer };
          ((Index == SequenceIndex && (Dest =
  std::variant_alternative_t<SequenceIndex, Variant>::Serialize(Reader), true))
  || ...);
  }*/

  void (*NetworkPacketHandler)(CHAR *Buffer, DWORD BufferSize,
                               uint8_t BufferHeader,
                               void *Context) = [](CHAR *Buffer,
                                                   DWORD BufferSize,
                                                   uint8_t BufferHeader,
                                                   void *Context) {
    std::vector<WinForge *> *PacketContext =
        reinterpret_cast<std::vector<WinForge *> *>(Context);
    switch (BufferHeader) {
    case OmniNet::PacketType::ChunkEnd: {
      // zeroth window since i'm still implenting multi window creation
      OmniNet::OmniHeader *header =
          reinterpret_cast<OmniNet::OmniHeader *>((Buffer + BufferSize - 3));
      auto *target = (*PacketContext)[header->Target];
      target->SetBufferData(Buffer, BufferSize);
      target->SetRenderEvent();
      break;
    }
    case OmniNet::Command: {
      OmniNet::OmniHeader *header =
          reinterpret_cast<OmniNet::OmniHeader *>((Buffer + BufferSize - 3));
      if (header->Flags == OmniNet::VoidArg) {
        OmniAPI::ExecuteNetCommand(*reinterpret_cast<CoreCommands *>(Buffer));
      } else {

        ByteStreamReader Reader{static_cast<uint32_t>(BufferSize - 3),
                                reinterpret_cast<uint8_t *>(Buffer)};

        OmniNetCommand Payload = OmniNetCommand::Deserialize(Reader);

        OmniCommand command{Payload};

        NetVariantDeserializer(
            command.Args, command.ArgTypeIndex,
            std::make_index_sequence<std::variant_size_v<FuncArgTypes>>(),
            Payload.Args.data(), Payload.Args.size());

        OmniAPI::ExecuteNetCommandWArgs(command);
      }
      break;
    }
    case OmniNet::PacketType::ProcMouse: {
      INPUT *Payload = reinterpret_cast<INPUT *>(Buffer);
      OmniSynth::ProcInput(*Payload);

      break;
    }

    case OmniNet::ProcKey: {
      INPUT *Payload = reinterpret_cast<INPUT *>(Buffer);
      OmniSynth::ProcInput(*Payload);
      break;
    }
    }
  };

  // helper funcs

  inline void TCharCpy(TCHAR(&Tarr), char(&arr), const size_t size) {
#ifndef UNICODE
    strcpy(&arr, &Tarr);
#else
    WideCharToMultiByte(CP_ACP, 0, &Tarr, -1, &arr, size, nullptr, nullptr);
#endif

    (&arr)[size] = '\0';
  };

  inline void IP2Char(const uint32_t IP, char *array) {
    std::sprintf(array, "%u.%u.%u.%u", (IP >> 24) & 0xFF, (IP >> 16) & 0xFF,
                 (IP >> 8) & 0xFF, IP & 0xFF);
  }

protected:
  HINSTANCE hInstance;
  int nCmdShow;

  HANDLE *Events = nullptr;
  DWORD EventDW = NULL;

  Instances *InstanceProbe = nullptr;

  std::mutex Mutex;

  std::unordered_map<DeviceMap, OmniInstance> AllInstances = {
      {DeviceMap::LU1, OmniInstance(5)}, {DeviceMap::U1, OmniInstance(2)},
      {DeviceMap::RU1, OmniInstance(6)}, {DeviceMap::L1, OmniInstance(1)},
      {DeviceMap::C0, OmniInstance(0)},  {DeviceMap::R1, OmniInstance(3)},
      {DeviceMap::LD1, OmniInstance(8)}, {DeviceMap::D1, OmniInstance(4)},
      {DeviceMap::RD1, OmniInstance(7)}};

  std::unordered_map<uint32_t, DeviceMap> InstanceLookup = {};

  std::unordered_map<DeviceMap, OmniActiveInstance> ActiveInstances;

  OmniCap OmniCap{ActiveInstances};
  OmniSynth OmniSynth;

  sessions sessions;
  uint8_t SessionCount = 0;

  ID3D11Device *D3D11Device = nullptr;
  ID3D11DeviceContext *D3D11Context = nullptr;
  IDXGISwapChain3 *swapchain = nullptr;
  ID3D11RenderTargetView *renderTargetView = nullptr;

  NVENCODER *Nv = nullptr;

  WGScreenCapture *WGSCapture = nullptr;
  ID3D11Texture2D *WGSCapBuffer = nullptr;
  bool WGCStatus = false;

  DXGICapture *DXGICap = nullptr;
  ID3D11Texture2D *DXGIBuffer = nullptr;
  bool DXGIStatus = false;

  DeviceMap SelectedInstance = DeviceMap::L1;
  std::vector<WinForge *> ActiveWindows{};
};

static void NvencOutputTest(NV_ENC_LOCK_BITSTREAM &NVBitstreamLock,
                            const char *baseName) {
  static uint64_t frameIndex = 0;

  if (!NVBitstreamLock.bitstreamBufferPtr ||
      NVBitstreamLock.bitstreamSizeInBytes == 0) {
    OutputDebugString(L"NvencOutputTest: empty bitstream\n");
    return;
  }

  char fileName[512];
  std::snprintf(fileName, sizeof(fileName), "%s_%llu.h264", baseName,
                static_cast<unsigned long long>(frameIndex++));

  FILE *outFile = std::fopen(fileName, "wb");
  if (outFile) {
    std::fwrite(NVBitstreamLock.bitstreamBufferPtr, 1,
                NVBitstreamLock.bitstreamSizeInBytes, outFile);
    std::fclose(outFile);
  } else {
    OutputDebugString(L"Failed to open output file\n");
  }
}

static int index = 0;

class OmniLink : public OmniCore {
public:
  OmniLink(HINSTANCE hInst, int nCmdShow);

  void OmniMain(HINSTANCE hInstance, int nCmdShow);

  void ToggleWGC();

  void ToggleDDAPI();

  void ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index);

  inline static void WGCapSend(session *session, WGScreenCapture *WGSCapture,
                               NvencSession *Nvs) {
    WGSCapture->WriteStateLock();

    Nvs->Encode();

    WGSCapture->WriteStateUnlock();

    // OutputDebugStringA((std::to_string(Nvs->NVBitstreamLock.bitstreamSizeInBytes)
    // + "\n").c_str());

    session->ChunkedSend(
        reinterpret_cast<char *>(Nvs->NVBitstreamLock.bitstreamBufferPtr),
        Nvs->NVBitstreamLock.bitstreamSizeInBytes);

    Nvs->NVUnlockBitStream();
  }

  inline static void DXGICapSend(session *session, DXGICapture *DXGICap,
                                 NvencSession *Nvs) {
    if (DXGICap->CaptureDXGI() == 0) {
      Nvs->Encode();

      // OutputDebugStringA((std::to_string(Nvs->NVBitstreamLock.bitstreamSizeInBytes)
      // + "\n").c_str());

      // NvencOutputTest(Nvs->NVBitstreamLock, index+"hellow there");

      session->ChunkedSend(
          reinterpret_cast<char *>(Nvs->NVBitstreamLock.bitstreamBufferPtr),
          Nvs->NVBitstreamLock.bitstreamSizeInBytes);

      Nvs->NVUnlockBitStream();
    }
  }

  inline void CommandListEmpty() {}

private:
  // GUI
  OmniGUI *GUI = nullptr;

  HWND hwnd = 0;

  NOTIFYICONDATAW TrayIconData = {};

  std::chrono::steady_clock::duration FrameTimeLimit =
      std::chrono::nanoseconds(15 * 1000000);

  std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
      std::chrono::steady_clock::now();

  float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

  void (OmniLink::*ExecuteCommand)() = &OmniLink::CommandListEmpty;

  static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam);

  // Active Input Proc Target Device
  static DeviceMap ActiveIOProcTarget;
  static DeviceMap SelectedTargetDevice;

  MSG msg = {};

  OmniShield InputFilter;

  NvencSession *NvencSessionPtr = nullptr;

  AsyncWorker::Uncached AsynLink;

  void OmniMainLoop();

  void InitTrayIcon(HWND hwnd);
};

#endif
