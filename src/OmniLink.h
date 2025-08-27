#ifndef UNICODE
#define UNICODE
#endif

#ifndef OMNILINK_H
#define OMNILINK_H

#pragma once

#include "SessionHandler.h"
#include "RendererCore.h"
#include "nvenc.h"
#include "nvdec.h"
#include "WinForge.h"
#include "WinCap.h"
#include "IOLink.h"
#include "OmniGUI.h"
#include "OmniLogger.h"

#include <Windows.h>
#include <dwmapi.h>
#pragma comment (lib, "dwmapi.lib")
#include <shellapi.h>
#include <string>
#include <vector>
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



struct OmniDevice {

	session DeviceSession;
	HWND ActiveWindow;
};


class OmniCore {
protected:
	HANDLE* Events = nullptr;
	DWORD EventDW = NULL;

	OmniCap OmniCap;
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


class OmniLink : public OmniCore{
public:
	
	session* session1 = nullptr;

	
	ComPtr<ID3D11Texture2D> NvencBuffer;



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
	OmniGUI OmniGUI;
	std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::steady_clock::duration(15 * 1000000);
	std::chrono::time_point<std::chrono::steady_clock> LastFrameTime = std::chrono::steady_clock::now();

	

	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	
	WinForge* Link = nullptr;

	MSG msg = { };
	
	void (OmniLink::*ExecuteCommand)() = &OmniLink::CommandListEmpty;

	static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void OmniMainLoop();

	static void PanelRendererSwitch(HWND hwnd);
};

#endif