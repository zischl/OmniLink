#ifndef RENDERER_H
#define RENDERER_H

#pragma warning(default : 4710)
#pragma warning(default : 4711)
#pragma warning(default : 4714)

#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <wrl/client.h>
#include <iostream>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;


struct D3DDevice {
	ComPtr<ID3D11Device> D3D11Device = nullptr;
	ComPtr<ID3D11DeviceContext> D3D11Context = nullptr;
};


struct SwapChainConfig {
	DXGI_FORMAT Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	BOOL Stereo = false;
	DXGI_SAMPLE_DESC SampleDesc = {1, 0};
	DXGI_USAGE BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	UINT BufferCount = 2;
	DXGI_SCALING Scaling = DXGI_SCALING_NONE;
	DXGI_SWAP_EFFECT SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	DXGI_ALPHA_MODE AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	UINT Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
};



struct VertexBufferConfig {
	D3D11_USAGE Usage = D3D11_USAGE_DEFAULT;
	UINT BindFlags = D3D11_BIND_VERTEX_BUFFER;
	UINT CPUAccessFlags = 0;
	UINT MiscFlags = 0;
	UINT StructureByteStride = 0;
	UINT SysMemPitch = 0;
	UINT SysMemSlicePitch = 0;
};

struct IndexBufferConfig {
	D3D11_USAGE Usage = D3D11_USAGE_DEFAULT;
	UINT BindFlags = D3D11_BIND_INDEX_BUFFER;
	UINT CPUAccessFlags = 0;
	UINT MiscFlags = 0;
	UINT StructureByteStride = 0;
	UINT SysMemPitch = 0;
	UINT SysMemSlicePitch = 0;
};





class Renderer {
private:
	HRESULT hr = S_OK;
	ComPtr<ID3DBlob> VertexShaderBlobTemp = nullptr;
	

	void _createCustomBuffer(ComPtr<ID3D11Device> D3D11Device, ComPtr<ID3D11Texture2D>& customBuffer, int bufferWidth, int bufferHeight);

public:
	Renderer();

	void SetViewPort(ID3D11DeviceContext* D3D11Context);

	D3DDevice CreateD3d11Device(D3D_FEATURE_LEVEL(FeatureLevels)[], UINT FeatureLevelCount, UINT& CreationFlags);

	inline void Renderer::GetDeferredContext(ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context) {
		hr = D3D11Device->CreateDeferredContext(0, &D3D11Context);
		if (FAILED(hr)) {
			OutputDebugStringA("Deferred Context Creation Failed!\n");
		}

	}
	ComPtr<IDXGIFactory2> CreateDXGIFactory2();

	ComPtr<IDXGISwapChain3> CreateSwapChain(
		ID3D11Device* D3D11Device,
		HWND& hwnd,
		UINT Width,
		UINT Height,
		IDXGIFactory2* Factory2,
		SwapChainConfig SwapChainConfig = {}
	);

	ComPtr<ID3D11Texture2D> GetSwapChainBuffer(IDXGISwapChain3 * SwapChain, UINT Buffer = 0);

	std::vector<ComPtr<ID3D11Texture2D>> GetSwapChainBuffersArray(IDXGISwapChain3* SwapChain, UINT Count);

	ComPtr<ID3D11RenderTargetView> CreateRTV(ID3D11Device* D3D11Device, ID3D11Texture2D* targetBuffer);

	std::vector <ComPtr<ID3D11RenderTargetView>> CreateRTVArray(ID3D11Device* D3D11Device, IDXGISwapChain3* SwapChain, UINT Count);

	ComPtr<ID3D11PixelShader> CreatePixelShader(ID3D11Device* D3D11Device, LPCWSTR FileName);

	ComPtr<ID3D11VertexShader> CreateVertexShader(ID3D11Device* D3D11Device, LPCWSTR FileName);

	ComPtr<ID3D11InputLayout> CreateInputLayout(ID3D11Device* D3D11Device, D3D11_INPUT_ELEMENT_DESC* layout, UINT ArraySize);
	
	template <typename VertexStruct, size_t size> 
	ComPtr<ID3D11Buffer> CreateVertexBuffer(
			ID3D11Device* D3D11Device, 
			VertexStruct(&Vertices)[size], 
			VertexBufferConfig BufferConfig = {}
	) {

		D3D11_BUFFER_DESC VertexBufferDesc = {
			sizeof(Vertices),
			BufferConfig.Usage,
			BufferConfig.BindFlags,
			BufferConfig.CPUAccessFlags,
			BufferConfig.MiscFlags,
			BufferConfig.StructureByteStride
		};

		D3D11_SUBRESOURCE_DATA initBufferData = { Vertices, 0, 0 };

		ComPtr<ID3D11Buffer> vertexBuffer;
		D3D11Device->CreateBuffer(&VertexBufferDesc, &initBufferData, &vertexBuffer);

		return vertexBuffer;
	};
	
	ComPtr<ID3D11Buffer> CreateIndexBuffer(ID3D11Device* D3D11Device, const unsigned short (&Indices)[], UINT ArraySize, IndexBufferConfig BufferConfig = {});

	
	

};

#endif