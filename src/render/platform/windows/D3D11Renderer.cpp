#include <D3D11Renderer.h>


void D3D11Renderer::RendererInit(HWND hwnd, int wdWidth, int wdHeight, HWNDxD3D11& RendererPtrStruct) {

	if (RendererPtrStruct.D3D11Device == nullptr) {
		D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

		D3DDevice DeviceStruct = CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);
		RendererPtrStruct.D3D11Device = DeviceStruct.D3D11Device;

		RendererPtrStruct.D3D11Context = DeviceStruct.D3D11Context;
	}

	if (RendererPtrStruct.D3D11Context == nullptr) {
		GetDeferredContext(RendererPtrStruct.D3D11Device.Get(), RendererPtrStruct.D3D11Context.Get());
	}
	else if (RendererPtrStruct.D3D11Context->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE) {

	}
	
	ComPtr<IDXGIFactory2> Factory2 = CreateDXGIFactory2();

	RendererPtrStruct.swapchain = CreateSwapChain(RendererPtrStruct.D3D11Device.Get(), hwnd, wdWidth, wdHeight, Factory2.Get());
	IDXGISwapChain3* swapchain = RendererPtrStruct.swapchain.Get();

	ComPtr<ID3D11Texture2D> SwapBuffer = GetSwapChainBuffer(swapchain, 0);

	RendererPtrStruct.renderTargetView = CreateRTV(RendererPtrStruct.D3D11Device.Get(), SwapBuffer.Get());

	return;
}


HWNDxShaders D3D11Renderer::ShadersInit(ID3D11Device* D3D11Device) {

	HWNDxShaders Shaders;

	Shaders.pixelShader = CreatePixelShader(D3D11Device, L"PixelShader.cso");
	ID3D11PixelShader* pixelShader = Shaders.pixelShader.Get();

	Shaders.vertexShader = CreateVertexShader(D3D11Device, L"VertexShader.cso");
	ID3D11VertexShader* vertexShader = Shaders.vertexShader.Get();

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	struct VertexStruct {
		DirectX::XMFLOAT3 POSITION;
		DirectX::XMFLOAT2 TEXCOORD;
	};

	Shaders.VertexBufferStride = sizeof(VertexStruct);
	Shaders.VertexBufferOffset = 0;

	VertexStruct vertices[] = {
		{ DirectX::XMFLOAT3(-1.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
		{ DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
		{ DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
		{ DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }
	};


	Shaders.inputLayout = CreateInputLayout(D3D11Device, layout, ARRAYSIZE(layout));

	Shaders.vertexBuffer = CreateVertexBuffer(D3D11Device, vertices);

	const unsigned short Indices[] =
	{
		0, 1, 2, 3, 4, 5
	};

	Shaders.IndexBuffer = CreateIndexBuffer(D3D11Device, { 0, 1, 2, 3, 4, 5 }, ARRAYSIZE(Indices));


	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.MaxAnisotropy = 1;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	D3D11Device->CreateSamplerState(&sampDesc, &Shaders.sampler);



	return Shaders;
}


void D3D11Renderer::SetShaders(ID3D11DeviceContext* D3D11Context, HWNDxShaders* shaders) {
	D3D11Context->IASetInputLayout(shaders->inputLayout.Get());
	D3D11Context->IASetVertexBuffers(0, 1, shaders->vertexBuffer.GetAddressOf(), &shaders->VertexBufferStride, &shaders->VertexBufferOffset);
	D3D11Context->IASetIndexBuffer(shaders->IndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
	D3D11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	D3D11Context->PSSetShader(shaders->pixelShader.Get(), nullptr, 0);
	D3D11Context->VSSetShader(shaders->vertexShader.Get(), nullptr, 0);
	D3D11Context->PSSetSamplers(0, 1, shaders->sampler.GetAddressOf());
}
