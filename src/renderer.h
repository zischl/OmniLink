#ifndef RENDERER_H
#define RENDERER_H

#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

class Renderer {
private:
	D3D_FEATURE_LEVEL selectedFeatureLevel;
	ComPtr<ID3D11Device> D3D11Device;
	ComPtr<ID3D11DeviceContext> D3D11Context;
	ComPtr<IDXGIFactory> factory;
	ComPtr<ID3D11RenderTargetView> renderTargetView = nullptr;
	ComPtr<ID3D11Texture2D> mainBuffer;
	ComPtr<IDXGISwapChain1> swapchain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapchainConfig = {};
	void _createCustomBuffer(ComPtr<ID3D11Device> D3D11Device, ComPtr<ID3D11Texture2D>& customBuffer, int bufferWidth, int bufferHeight);

public:
	Renderer(HWND hwnd, int width, int height, bool swap);
	void CreateRTV(ID3D11Device* D3D11Device, ID3D11Texture2D* targetBuffer);
	ComPtr<ID3D11Device> getDevice() const;
	ComPtr<ID3D11DeviceContext> getContext() const;
	ComPtr<IDXGISwapChain> getSwapchain() const;
	ComPtr<ID3D11RenderTargetView> getRTV() const;
	ComPtr<ID3D11Texture2D> getMainBuffer() const;
};

#endif