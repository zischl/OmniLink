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



class OmniCore {

public:
	//Core Functions


	void WinGetUserName(char(&CharArray)[UNLEN + 1]);

	void WinGetComputerName(char(&CharArray)[MAX_COMPUTERNAME_LENGTH + 1]);

	uint32_t QueryLocalIP(const int index = 0);

	void ScanInstances();

	void SwapInstanceLayout(int index1, int index2);

	std::array<OmniInstance, 5>* GetAvailableInstances() noexcept;


	//Command Queue System
	std::array<void (OmniCore::* )(), 10> CommandTable = {
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



	//helper funcs

	inline void TCharCpy(TCHAR (&Tarr), char(&arr), const size_t size) 
	{
#ifndef UNICODE
		strcpy(&arr, &Tarr);
#else
		WideCharToMultiByte(CP_ACP, 0, &Tarr, -1, &arr, size, nullptr, nullptr);
#endif

		(&arr)[size] = '\0';
	}


protected:

	HANDLE* Events = nullptr;
	DWORD EventDW = NULL;

	OmniCap OmniCap;
	Instances* InstanceProbe = nullptr;
	std::mutex Mutex;
	std::array<OmniInstance, 5> AllInstances;
	//OmniInstance ActiveInstances[4];
	sessions sessions;

	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* D3D11Context = nullptr;
	IDXGISwapChain3* swapchain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;



	NVENCODER* Nv = nullptr;

	WGScreenCapture* WGSCapture = nullptr;
	ID3D11Texture2D* WGSCapBuffer = nullptr;
	bool WGCStatus = false;

	DXGICapture* DXGICap = nullptr;
	ComPtr<IDXGIOutputDuplication> DXGIOutDuplication;
	ID3D11Texture2D* DXGIBuffer = nullptr;
	bool DXGIStatus = false;

};


class OmniLink : public OmniCore {
public:
	session* session1 = nullptr;

	WinForge* Link = nullptr;

	void OmniMain(HINSTANCE hInstance, int nCmdShow);
	int test2(HINSTANCE hInstance, int nCmdShow);

	void ToggleWGC();

	inline void WGCapSend() {
		if (WGSCapture != nullptr)
			WGSCapture->WriteStateLock();

		Nv->Encode();

		WGSCapture->WriteStateUnlock();

		session1->ChunkedSend(reinterpret_cast<char*>(Nv->NVBitstreamLock.bitstreamBufferPtr), Nv->NVBitstreamLock.bitstreamSizeInBytes);

		Nv->NVUnlockBitStream();
	}

	void ToggleDDAPI();

	inline void DXGICapSend() {
		if (DXGICap->CaptureDXGI() == 0) {

			Nv->Encode();

			session1->ChunkedSend(reinterpret_cast<char*>(Nv->NVBitstreamLock.bitstreamBufferPtr), Nv->NVBitstreamLock.bitstreamSizeInBytes);

			Nv->NVUnlockBitStream();
			DXGIOutDuplication->ReleaseFrame();

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

	MSG msg = { };

	void (OmniLink::* ExecuteCommand)() = &OmniLink::CommandListEmpty;

	static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


	//Streamer Links Window Proc
	static LRESULT CALLBACK WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);



	void OmniMainLoop();

	static void PanelRendererSwitch(HWND hwnd);

};

#endif