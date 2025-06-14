#include "renderer.h"

#include <Windows.h>
#include <string>
#include <wrl/client.h>
#include <iostream>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;


void Renderer::_createCustomBuffer(ComPtr<ID3D11Device> D3D11Device, ComPtr<ID3D11Texture2D>& customBuffer, int bufferWidth, int bufferHeight) {
	D3D11_TEXTURE2D_DESC customBufferDesc = {};
	customBufferDesc.Width = static_cast<UINT>(bufferWidth);
	customBufferDesc.Height = static_cast<UINT>(bufferHeight);
	customBufferDesc.MipLevels = 1;
	customBufferDesc.ArraySize = 1;
	customBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	customBufferDesc.SampleDesc.Count = 1;
	customBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	customBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT hr = D3D11Device->CreateTexture2D(&customBufferDesc, nullptr, customBuffer.GetAddressOf());
	
}

Renderer::Renderer(HWND hwnd, int bufferWidth, int bufferHeight, bool swap) {
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	#if defined(_DEBUG)
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	#endif

	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, creationFlags,
		featureLevels, _countof(featureLevels), D3D11_SDK_VERSION,
		&D3D11Device, &selectedFeatureLevel, &D3D11Context);
	if (FAILED(hr)) {
		OutputDebugString("D3D11 Device Creation Failed.");
	}

	hr = CreateDXGIFactory(IID_PPV_ARGS(&factory));
	if (FAILED(hr)) {
		OutputDebugString("Factory Creation Failed.");
	}

	IDXGIFactory2* Factory2 = nullptr;
	hr = factory->QueryInterface(__uuidof(IDXGIFactory2), (void**)&Factory2);


	

	if (swap) {
		swapchainConfig.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		swapchainConfig.BufferCount = 2;
		swapchainConfig.Width = 1920;
		swapchainConfig.Height = 1080;
		swapchainConfig.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		swapchainConfig.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainConfig.SampleDesc.Count = 1;
		swapchainConfig.Scaling = DXGI_SCALING_NONE;
		swapchainConfig.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		swapchainConfig.Stereo = false;
		swapchainConfig.SampleDesc.Count = 1;
		swapchainConfig.SampleDesc.Quality = 0;

		
		

		hr = Factory2->CreateSwapChainForHwnd(D3D11Device.Get(), hwnd, &swapchainConfig, nullptr, nullptr, &swapchain);
		if (FAILED(hr)) {
			OutputDebugString("Swapchain Creation Failed.");
		}

		hr = swapchain->GetBuffer(0, IID_PPV_ARGS(&mainBuffer));
		if (FAILED(hr)) {
			OutputDebugString("Swapchain Get Buffer Failed.");
		}
	}
	else {
		_createCustomBuffer(D3D11Device, mainBuffer, bufferWidth, bufferHeight);
	}
	
	//CreateRTV(D3D11Device.Get(), mainBuffer.Get());
};


void Renderer::CreateRTV(ID3D11Device* D3D11Device, ID3D11Texture2D* targetBuffer) {
	HRESULT hr = D3D11Device->CreateRenderTargetView(targetBuffer, nullptr, &renderTargetView);
	if (FAILED(hr)) {
		OutputDebugString("RTV Creation Failed.");
	}
}


ComPtr<ID3D11Device> Renderer::getDevice() const { return D3D11Device; }
ComPtr<ID3D11DeviceContext> Renderer::getContext() const { return D3D11Context; }
ComPtr<IDXGISwapChain> Renderer::getSwapchain() const { return swapchain; }
ComPtr<ID3D11RenderTargetView> Renderer::getRTV() const { return renderTargetView; }
ComPtr<ID3D11Texture2D> Renderer::getMainBuffer() const { return mainBuffer; }


