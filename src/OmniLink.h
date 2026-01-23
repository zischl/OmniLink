#ifndef UNICODE
#define UNICODE
#endif

#ifndef OMNILINK_H
#define OMNILINK_H

#pragma once

#include "OmniAPI.h"
#include "OmniTypes.h"
#include "SessionHandler.h"
#include "RendererCore.h"
#include "nvenc.h"
#include "nvdec.h"
#include "WinForge.h"
#include "WinCap.h"
#include "IOLink.h"
#include "OmniDiscovery.h"
#include "OmniLogger.h"
#include "OmniGUI.h"
#include <Windows.h>
#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")
#include <shellapi.h>
#include <string>
#include <vector>
#include <deque>
#include <variant>
#include <array>
#include <wrl/client.h>
#include <iostream>
#include <fstream>

#include <d3d11.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <dcomp.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dcomp.lib")
#include <wincodec.h>
#include <comdef.h>

#include <lz4.h>

#ifndef OMNI_BUILD_RELEASE
#pragma comment(lib, "lz4d.lib")
#endif

#ifndef OMNI_BUILD_DEBUG
#pragma comment(lib, "lz4.lib")
#endif

#include <thread>
#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")

#define WM_TRAYICON (WM_USER + 1)

#define OmniPort 62485
#define MTU 1450


class OmniCore {

public:

	inline void OmniCmdStatus() {
		Logger::log("CMD Queue Status Test\n");
	}

	//Core Functions


	void WinGetUserName(char(&CharArray)[UNLEN + 1]);

	void WinGetComputerName(char(&CharArray)[OmniDevNameLen + 1]);

	void QueryLocalIP(uint32_t& LocalIP, const int index = 0);

	void ScanInstances();

	void Connect(char IP[16], char Auth[4]);

	void ConnectInstance(DeviceMap index);

	void SwapInstanceLayout(int index1, int index2);

	std::unordered_map<DeviceMap, OmniInstance>* GetAvailableInstances() noexcept;


	//Command Queue System
	std::array<void (OmniCore::*)(), 10> CommandTable = {
		&OmniCore::OmniCmdStatus,
		&OmniCore::ScanInstances

	};

	/// <summary>
	/// This defines the maximum number of commands that can be queued using PushCommand 
	/// functions and drained with ExecuteCommandQueue.
	/// </summary>
	BurstQ<CoreCommands, 20> CommandBurstQ = BurstQ<CoreCommands, 20>();

	/// <summary>
	/// Same as above but for commands with args, memory taken up by the queue will be based 
	/// on the biggest size argument structure defined in FuncArgTypes.
	/// </summary>
	BurstQ<FuncArgTypes, 20> CommandBurstQWArgs = BurstQ<FuncArgTypes, 20>();

	inline void ExecuteCommandQueue()
	{
		SetEvent(Events[4]);
	}

	inline void ExecuteCommandQueueWArgs()
	{
		SetEvent(Events[5]);
	}

	inline void PushCommand(CoreCommands CommandType)
	{
		CommandBurstQ.push(CommandType);

	}

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

	inline void PushNetworkCommand()
	{

	}



	void (*NetworkPacketHandler)(CHAR* Buffer, DWORD BufferSize, uint8_t BufferHeader, void* Context) = [](CHAR* Buffer, DWORD BufferSize, uint8_t BufferHeader, void* Context)
		{
			OmniActiveInstance* UserInstance = reinterpret_cast<OmniActiveInstance*>(Context);
			switch (BufferHeader)
			{
			case OmniNet::PacketType::ChunkEnd:
				//zeroth window since i'm still implenting multi window creation
				UserInstance->ActiveWindows[0]->SetBufferData(Buffer, BufferSize);
				UserInstance->ActiveWindows[0]->SetRenderEvent();
				break;

			case OmniNet::Command:
			{
				OmniNet::OmniHeader* header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
				if (header->Flags == OmniNet::VoidArg)
				{
					OmniAPI::ExecuteNetCommand(*reinterpret_cast<CoreCommands*>(Buffer));
				}
				else
				{
					OmniNetCommandType* Payload = reinterpret_cast<OmniNetCommandType*>(Buffer);

					OmniCommand Command;
					Command.CommandType = Payload->CommandType;
					Command.ArgTypeIndex = Payload->ArgTypeIndex;
					Variance::VariantDeserializer<FuncArgTypes>
						(
							Command.Args,
							Payload->ArgTypeIndex,
							std::make_index_sequence<std::variant_size_v<FuncArgTypes>>{},
							Payload->Args
						);
				}
				break;
			}
			case OmniNet::PacketType::ProcMouse:
			{
				OutputDebugStringA("Receiving Mouse Inputs");
				MouseXY* Payload = reinterpret_cast<MouseXY*>(Buffer);
				OmniSynth::ProcMouse(Payload->X, Payload->Y);
				break;

			}

			case OmniNet::ProcKey:
			{
				RAWINPUT* Payload = reinterpret_cast<RAWINPUT*>(Buffer);
				//OmniSynth::ProcKey(*Payload);
				break;

			}
			}
		};


	//helper funcs

	inline void TCharCpy(TCHAR(&Tarr), char(&arr), const size_t size)
	{
#ifndef UNICODE
		strcpy(&arr, &Tarr);
#else
		WideCharToMultiByte(CP_ACP, 0, &Tarr, -1, &arr, size, nullptr, nullptr);
#endif

		(&arr)[size] = '\0';
	};


	inline void IP2Char(const uint32_t IP, char* array)
	{
		std::sprintf(array, "%u.%u.%u.%u", (IP >> 24) & 0xFF, (IP >> 16) & 0xFF, (IP >> 8) & 0xFF, IP & 0xFF);
	}

protected:

	HANDLE* Events = nullptr;
	DWORD EventDW = NULL;

	OmniCap OmniCap;
	OmniSynth OmniSynth;
	std::mutex Mutex;
	Instances* InstanceProbe = nullptr;

	std::unordered_map<DeviceMap, OmniInstance> AllInstances = {
		{ DeviceMap::LU1, OmniInstance(5) }, { DeviceMap::U1, OmniInstance(2) }, { DeviceMap::RU1, OmniInstance(6) },
		{ DeviceMap::L1, OmniInstance(1) }, { DeviceMap::C0, OmniInstance(0) }, { DeviceMap::R1, OmniInstance(3) },
		{ DeviceMap::LD1, OmniInstance(8) }, { DeviceMap::D1, OmniInstance(4) }, { DeviceMap::RD1, OmniInstance(7) }
	};

	std::unordered_map<uint32_t, DeviceMap> InstanceLookup = {};

	std::unordered_map<DeviceMap, OmniActiveInstance> ActiveInstances;
	sessions sessions;
	uint8_t SessionCount = 0;

	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* D3D11Context = nullptr;
	IDXGISwapChain3* swapchain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;



	NVENCODER* Nv = nullptr;

	WGScreenCapture* WGSCapture = nullptr;
	ID3D11Texture2D* WGSCapBuffer = nullptr;
	bool WGCStatus = false;

	DXGICapture* DXGICap = nullptr;
	ID3D11Texture2D* DXGIBuffer = nullptr;
	bool DXGIStatus = false;


	DeviceMap SelectedInstance = DeviceMap::L1;

};






class OmniLink : public OmniCore {
public:

	void OmniMain(HINSTANCE hInstance, int nCmdShow);

	void ToggleWGC();

	void ToggleDDAPI();

	void ToggleFeature(DeviceMap Index, int FeatureIndex);

	inline static void WGCapSend(session* session, WGScreenCapture* WGSCapture, NVENCODER* Nv) {
		WGSCapture->WriteStateLock();

		//Nv->Encode();

		WGSCapture->WriteStateUnlock();

		//session->ChunkedSend(reinterpret_cast<char*>(Nv->NVBitstreamLock.bitstreamBufferPtr), Nv->NVBitstreamLock.bitstreamSizeInBytes);

		//Nv->NVUnlockBitStream();

	}

	inline static void DXGICapSend(session* session, DXGICapture* DXGICap, NVENCODER* Nv) {
		if (DXGICap->CaptureDXGI() == 0) {

			//Nv->Encode();
			//session->ChunkedSend(reinterpret_cast<char*>(Nv->NVBitstreamLock.bitstreamBufferPtr), Nv->NVBitstreamLock.bitstreamSizeInBytes);
		}

	}

	inline void CommandListEmpty() {
	}

private:
	//GUI
	OmniGUI* GUI = nullptr;

	std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(15 * 1000000);

	std::chrono::time_point<std::chrono::steady_clock> LastFrameTime = std::chrono::steady_clock::now();

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	void (OmniLink::* ExecuteCommand)() = &OmniLink::CommandListEmpty;

	static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	//Streamer Links Window Proc
	static LRESULT CALLBACK WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


	//Active Input Proc Target Device
	static DeviceMap ActiveIOProcTarget;


	MSG msg = { };

	void OmniMainLoop();

	static void PanelRendererSwitch(HWND hwnd);

};

#endif