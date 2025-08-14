#ifndef UNICODE
#define UNICODE
#endif 

#ifndef WINFORGE_H
#define WINFORGE_H


#pragma once


#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <comdef.h>
#include <wrl/client.h>

#include <string>
#include <future>
#include <mutex>
#include <thread>

#include <OmniRenderer.h>
#include "WinCap.h"
#include <nvdec.h>

using Microsoft::WRL::ComPtr;

struct WinConfig {
	wchar_t class_name = L'Something';
	const wchar_t Window_Name;
	UINT wdWidth = 1280;
	UINT wdHeight = 720;
	LPVOID lParam = NULL;

	WinConfig(wchar_t ClassName, UINT Width, UINT Height, wchar_t WindowName, LPVOID lParam_) :
		class_name(ClassName),
		Window_Name(WindowName),
		wdWidth(Width),
		wdHeight(Height),
		lParam(lParam_) {}
};


HWND WindowInit(WinConfig& Config, HINSTANCE hInstance, int nCmdShow, WNDPROC WProc);

class WinForge {
public:
	WinForge(WNDPROC WindowProc);

	
	HWND CreateWindowAsync(wchar_t window_name, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct = {});
	
	inline void SetFrameBufferSize(int size) {
		for (int frame_index = 0; frame_index < FrameQueueSize; frame_index++) {
			FramePool[frame_index].FrameBuffer = new CHAR[FrameSize];
		}
	}

	inline void SetFramePoolSize(int size) {
		if (FramePool != nullptr) {
			delete[] FramePool;
		}

		FramePool = new Frame[size];
		SetFrameBufferSize(FrameSize);
	}

	inline void SetBufferData(char* data, int size) {
		memcpy(FramePool[NextFrame].FrameBuffer, data, size);
		
		//OutputDebugString((std::to_wstring(CurrentFrame) + L" " + std::to_wstring(CurrentFrame) + L"\n").c_str());
		FramePool[NextFrame].FrameSize = size;
		NextFrame = NextFrame + 1 & 3;
	}

	inline void DecodeBuffer() {
		NVDec->NVDecode(reinterpret_cast<const unsigned char*>(FramePool[CurrentFrame].FrameBuffer), FramePool[CurrentFrame].FrameSize);
		CurrentFrame = CurrentFrame + 1 & 3;
	}

	inline void SetRenderEvent() {
		SetEvent(Events[0]);
	}
	//void ContextSwitch();

	inline void SetFPSLimit(int FPS) {
		limit.store(1000 / FPS);
	}


private:
	HRESULT hr = NULL;
	HWND hwnd = NULL;
	WNDPROC WProc = NULL;
	HANDLE* Events = nullptr;
	DWORD EventDW = NULL;




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

	struct Frame{
		CHAR* FrameBuffer;
		UINT FrameSize = 0;
	};

	int FrameSize = 300000;
	int FrameQueueSize = 4;
	Frame* FramePool = nullptr;
	uint8_t CurrentFrame = 0;
	uint8_t NextFrame = 0;


	D3D11_DEVICE_CONTEXT_TYPE ContextMode = D3D11_DEVICE_CONTEXT_IMMEDIATE;

	std::atomic<int> limit = 7;
	MSG msg = { };

	
	void Render();
	void MainLoop();

	__forceinline void null() {}

};

#endif