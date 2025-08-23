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
#include <thread>
#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")

#define WM_TRAYICON (WM_USER + 1)



struct OmniDevice {

	session DeviceSession;
	HWND ActiveWindow;
};

class OmniCore {
private:
	OmniCap OmniCap;
	sessions sessions;
public:
	void Execute();

	void AddDevice();
};


class OmniLink {
public:
	HANDLE* Events = nullptr;
	DWORD EventDW = NULL;

	OmniCap OmniCap;
	sessions sessions;
	session* session1 = nullptr;

	ComPtr<IDXGIOutputDuplication> DXGIOutDuplication;
	ID3D11Texture2D* DXGIBuffer = nullptr;

	NVENCODER* Nv = nullptr;
	ComPtr<ID3D11Texture2D> NvencBuffer;



	void OmniMain(HINSTANCE hInstance, int nCmdShow);
	int test2(HINSTANCE hInstance, int nCmdShow);



private:
	OmniGUI OmniGUI;
	std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::steady_clock::duration(15 * 1000000);
	std::chrono::time_point<std::chrono::steady_clock> LastFrameTime = std::chrono::steady_clock::now();

	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* D3D11Context = nullptr;
	IDXGISwapChain3* swapchain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;

	float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	
	WinForge* Link = nullptr;

	MSG msg = { };
	
	WGScreenCapture WGSCapture;
	DXGICapture DXGICapture;

	static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void OmniMainLoop();

	static void PanelRendererSwitch(HWND hwnd);
};

#endif