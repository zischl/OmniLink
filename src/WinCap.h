
#ifndef WINCAP_H
#define WINCAP_H


#pragma once
#include <OmniLogger.h>

#include <d3d11.h>
#include <dxgi1_5.h>
#include <Windows.h>
#include <winrt/windows.graphics.capture.h>
#include <windows.graphics.capture.interop.h>
#include <winrt/Windows.Foundation.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#pragma comment(lib, "windowsapp.lib")

#include <wrl/client.h>




using Microsoft::WRL::ComPtr;

class DXGICapture {
private:
	HRESULT hr;

	ComPtr<IDXGIOutputDuplication> DXGIOutDuplication;

	DXGI_OUTDUPL_FRAME_INFO frameinfo;
	ComPtr<IDXGIResource> framepixeldata = nullptr;
	ComPtr<ID3D11Texture2D> DXGIComBuffer = nullptr;


public:
	ComPtr<IDXGIOutputDuplication> InitDXGI(ID3D11Device* D3D11Device);

	ID3D11Texture2D* GetBuffer() const;

	inline int CaptureDXGI() {
		return (DXGIOutDuplication->AcquireNextFrame(0, &frameinfo, &framepixeldata) == DXGI_ERROR_WAIT_TIMEOUT);
	}
};


class WGCapture {
public:

	winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice D3DDevice_WGC{ nullptr };
	
	ID3D11DeviceContext* D3D11Context = nullptr;

	inline void SetWrappedD3D11Device(ID3D11Device* D3D11DevicePtr) {
		ComPtr< ID3D11Device> ComID3D11Device = D3D11DevicePtr;
		ComPtr<IDXGIDevice> DXGIDevice;
		ComID3D11Device.As(&DXGIDevice);

		winrt::com_ptr<IInspectable> inspectableSurface;
		if (SUCCEEDED(CreateDirect3D11DeviceFromDXGIDevice(DXGIDevice.Get(), inspectableSurface.put())))
		{
			D3DDevice_WGC = inspectableSurface.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
		}

	}

	inline HMONITOR GetActiveMonitor() {
		HWND WindowHandle = GetForegroundWindow();
		HMONITOR ActiveMonitor = MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST);
		return ActiveMonitor;
	}

	inline void GetRoActivationFactory(void** WGCInterop) {
		HSTRING ClassName;

		winrt::check_hresult(
			WindowsCreateString(
				L"Windows.Graphics.Capture.GraphicsCaptureItem",
				wcslen(L"Windows.Graphics.Capture.GraphicsCaptureItem"),
				&ClassName
			)
		);

		winrt::check_hresult
		(
			RoGetActivationFactory
			(
				ClassName,
				__uuidof(IGraphicsCaptureItemInterop),
				WGCInterop
			)
		);
	}

	void GetActiveMonitorCaptureItem(winrt::Windows::Graphics::Capture::GraphicsCaptureItem& CaptureItem);

	void CreateWGCBuffer(ID3D11Device* D3D11Device, ID3D11Texture2D** Buffer);
	
};

class WGScreenCapture : public WGCapture{
private:
	HRESULT hr = S_OK;

	winrt::Windows::Graphics::Capture::GraphicsCaptureSession Session{ NULL };

	winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool FramePool{ NULL };

	winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem{ nullptr };

	std::atomic_bool WriteState{ false };

	winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame Frame{ nullptr };
	winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame ValidFrame{ nullptr };

	ID3D11Texture2D* WBuffer = nullptr;

	winrt::com_ptr<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> _SurfaceInterface;;

	ID3D11Texture2D* _SurfaceTexture = nullptr;


public:
	WGScreenCapture(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11Context_);

	void CreateMonitorCapSession(ID3D11Texture2D* Buffer, UINT Width, UINT Height);

	inline void WriteStateLock() {
		WriteState.store(true);
	}

	inline void WriteStateUnlock() {
		WriteState.store(false);
	}

	void StartSession();

	void CloseSession();


};

#endif