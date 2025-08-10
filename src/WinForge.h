#ifndef UNICODE
#define UNICODE
#endif 

#ifndef WINFORGE_H
#define WINFORGE_H


#pragma once

#include <OmniRenderer.h>
#include "WinCap.h"
#include <nvdec.h>

#include <string>
#include <future>
#include <mutex>
#include <thread>

#include <Windows.h>
#include <comdef.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct WinConfig {
	wchar_t class_name = L'Something';
	const wchar_t Window_Name;
	UINT wdWidth = 1280;
	UINT wdHeight = 720;
	LPVOID lParam = NULL;

	WinConfig(wchar_t ClassName, UINT Width, UINT Height, wchar_t WindowName, LPVOID lParam_) :
		class_name(ClassName),
		wdWidth(Width),
		wdHeight(Height),
		Window_Name(WindowName),
		lParam(lParam_) {}
};


class WinForge {
public:
	WinForge(WNDPROC WindowProc);

	HWND WindowInit(WinConfig& Config, HINSTANCE& hInstance, int nCmdShow);
	
	HWND CreateWindowAsync(wchar_t class_name, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct = {});

	void Render();  

	void ContextSwitch();

	inline void SetFPSLimit(int FPS)
	{
		limit.store(1000 / FPS);
	}
private:
	HRESULT hr = NULL;
	HWND hwnd = NULL;
	WNDPROC WProc = NULL;

	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* D3D11Context = nullptr;

	IDXGISwapChain3* swapchain = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;

	ID3D11PixelShader* pixelShader = nullptr;
	ID3D11VertexShader* vertexShader = nullptr;
	ID3D11Buffer* vertexBuffer = nullptr;
	ID3D11InputLayout* inputLayout = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	ID3D11SamplerState* sampler = nullptr;
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	ComPtr<ID3D11ShaderResourceView> textureView = nullptr;

	UINT stride = 0;
	UINT offset = 0;

	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

	D3D11_TEXTURE2D_DESC custommainBufferDesc = {};
	ComPtr<ID3D11Texture2D> NvdecBuffer;
	NVDecoder* NVDec = nullptr;

	CHAR* FrameBuffer[20];

	D3D11_DEVICE_CONTEXT_TYPE ContextMode = D3D11_DEVICE_CONTEXT_IMMEDIATE;

	std::atomic<int> limit = 7;
	MSG msg = { };

	void MainLoop();

	__forceinline void null() {}

};

#endif