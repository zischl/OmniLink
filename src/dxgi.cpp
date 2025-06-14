#ifndef UNICODE
#define UNICODE
#endif

#include <Windows.h>
#include <string>
#include <wrl/client.h>
#include <iostream>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include "renderer.h"
#include <dcomp.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dcomp.lib")
#include <vector>
#include <wincodec.h>
#include <comdef.h>



using Microsoft::WRL::ComPtr;

LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
	const wchar_t CLASS_NAME[] = L"Test Window";

	unsigned int wdWidth = 1920;
	unsigned int wdHeight = 1080;

	WNDCLASS wc = {};
	wc.lpfnWndProc = WProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"too ez",
		WS_OVERLAPPEDWINDOW,
		0,
		0,
		wdWidth,
		wdHeight,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd == NULL)
	{
		return 0;
	}

	ShowWindow(hwnd, nCmdShow);

	

	/*##############################################################*/

	Renderer Renderer(hwnd, wdWidth, wdHeight, true);
	ComPtr<ID3D11Device> D3D11Device = Renderer.getDevice();
	ComPtr<ID3D11DeviceContext> D3D11Context = Renderer.getContext();
	ComPtr<IDXGISwapChain> swapchain = Renderer.getSwapchain() ;
	ComPtr<ID3D11RenderTargetView> renderTargetView = Renderer.getRTV();
	ComPtr<ID3D11Texture2D> mainBuffer = Renderer.getMainBuffer();

	/*##############################################################*/

	ComPtr<IDXGIDevice> DXGIDevice;
	D3D11Device.As(&DXGIDevice);

	ComPtr<IDXGIAdapter> DXGIAdapter;
	DXGIDevice->GetAdapter(&DXGIAdapter);

	ComPtr<IDXGIOutput> DXGIOutput;
	DXGIAdapter->EnumOutputs(0, &DXGIOutput);

	ComPtr<IDXGIOutput1> DXGIOutputEnhanced;
	DXGIOutput.As(&DXGIOutputEnhanced);

	ComPtr<IDXGIOutputDuplication> DXGIOutDuplication;
	HRESULT hr = DXGIOutputEnhanced->DuplicateOutput(D3D11Device.Get(), &DXGIOutDuplication);

	/*##############################################################*/
	
	ComPtr<ID3DBlob> pixelShaderBlob = nullptr;
	D3DReadFileToBlob(L"PixelShader.cso", &pixelShaderBlob);

	ComPtr<ID3DBlob> vertexShaderBlob = nullptr;
	D3DReadFileToBlob(L"VertexShader.cso", &vertexShaderBlob);


	ComPtr<ID3D11PixelShader> pixelShader = nullptr;
	D3D11Device->CreatePixelShader(pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize(),
		nullptr, &pixelShader);

	ComPtr<ID3D11VertexShader> vertexShader = nullptr;
	D3D11Device->CreateVertexShader(vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(),
		nullptr, &vertexShader);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);


	ComPtr<ID3D11InputLayout> inputLayout;
	D3D11Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize(), &inputLayout);


	/*##############################################################*/

	struct Vertex {
		DirectX::XMFLOAT3 POSITION;
		DirectX::XMFLOAT2 TEXCOORD;
	};


	Vertex vertices[] = {
		{ DirectX::XMFLOAT3(-1.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
		{ DirectX::XMFLOAT3(1.0f, 1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
		{ DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
		{ DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) }
	};

	/*unsigned short triangleIndices[] =
	{
		0, 1, 2, 3, 4, 5
	};*/

	D3D11_BUFFER_DESC bd = { sizeof(vertices) * _countof(vertices), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0,0,0 };
	D3D11_SUBRESOURCE_DATA initData = { vertices, 0, 0 };

	ComPtr<ID3D11Buffer> vertexBuffer;
	D3D11Device->CreateBuffer(&bd, &initData, &vertexBuffer);

	/*D3D11_BUFFER_DESC indexBufferDesc;
	indexBufferDesc.ByteWidth = sizeof(unsigned short) * ARRAYSIZE(triangleIndices);
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;*/

	/*D3D11_SUBRESOURCE_DATA indexBufferData;
	indexBufferData.pSysMem = triangleIndices;
	indexBufferData.SysMemPitch = 0;
	indexBufferData.SysMemSlicePitch = 0;

	ComPtr<ID3D11Buffer> indexBuffer;
	D3D11Device->CreateBuffer(&indexBufferDesc, &indexBufferData, &indexBuffer);*/


	static ComPtr<ID3D11SamplerState> sampler;
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.MaxAnisotropy = 1;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;



	D3D11Device->CreateSamplerState(&sampDesc, &sampler);


	ComPtr<ID3D11ShaderResourceView> textureView = nullptr;


	float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

	ComPtr<ID3D11Texture2D> tempBuffer;

	D3D11_TEXTURE2D_DESC custommainBufferDesc = {};
	custommainBufferDesc.Width = wdWidth;
	custommainBufferDesc.Height = wdHeight;
	custommainBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	custommainBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	custommainBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	custommainBufferDesc.SampleDesc.Count = 1;
	custommainBufferDesc.SampleDesc.Quality = 0;
	custommainBufferDesc.ArraySize = 1;
	//custommainBufferDesc.MipLevels = 1;


	D3D11Device->CreateTexture2D(&custommainBufferDesc, nullptr, tempBuffer.GetAddressOf());

	hr = D3D11Device->CreateRenderTargetView(mainBuffer.Get(), nullptr, &renderTargetView);
	if (FAILED(hr)) {
		OutputDebugString(L"RTV Creation Failed.");
	}


	/*##############################################################*/



	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	
	UINT stride = sizeof(Vertex);
	UINT offset = 0;


	MSG msg = { };
	while (true) {

		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				return 0;

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

		DXGI_OUTDUPL_FRAME_INFO frameinfo;
		ComPtr<IDXGIResource> framepixeldata;
		//ComPtr<ID3D11Texture2D> frame;

		hr = DXGIOutDuplication->AcquireNextFrame(500, &frameinfo, &framepixeldata);
		if (SUCCEEDED(hr)) {
			framepixeldata.As(&tempBuffer);
			D3D11Context->ClearRenderTargetView(renderTargetView.Get(), clearColor);
			
			//swapchain->ResizeBuffers(16, wdWidth, wdHeight, DXGI_FORMAT_B8G8R8A8_UNORM, 0);


			//D3D11Context->CopyResource(test.Get(), tempBuffer.Get());
			/*D3D11Context->CopyResource(mainBuffer.Get(), frame.Get());
			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);*/

			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = wdWidth;
			viewport.Height = wdHeight;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			D3D11Context->RSSetViewports(1, &viewport);

			//LoadWICTexture(D3D11Device.Get(), D3D11Context.Get(), L"CapturedFrame.png", textureView, width, height);
			//SaveTextureToFile(tempBuffer.Get(), L"CapturedFrame.png", D3D11Context.Get());

			hr = D3D11Device->CreateShaderResourceView(tempBuffer.Get(), &srvDesc, textureView.GetAddressOf());
			if (FAILED(hr)) {
				_com_error err(hr);
				OutputDebugString(err.ErrorMessage());
				DXGIOutDuplication->ReleaseFrame();
				OutputDebugString(L"aaaaaaaaaaaaaaaa\n");
				continue;
			}


			D3D11Context->IASetInputLayout(inputLayout.Get());
			D3D11Context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
			//D3D11Context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
			D3D11Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

			D3D11Context->PSSetShader(pixelShader.Get(), nullptr, 0);
			D3D11Context->VSSetShader(vertexShader.Get(), nullptr, 0);
			D3D11Context->PSSetShaderResources(0, 1, textureView.GetAddressOf());
			D3D11Context->PSSetSamplers(0, 1, sampler.GetAddressOf());

			D3D11Context->OMSetRenderTargets(1, renderTargetView.GetAddressOf(), nullptr);
			D3D11Context->Draw(4, 0);
			///* ################################################################ */
				


			/*D3D11_BOX bbox = {};
			bbox.left = 0;
			bbox.top = 0;
			bbox.front = 0;
			bbox.right = min(1920, 1920);
			bbox.bottom = min(1080, 1080);
			bbox.back = 1;

			D3D11Context->CopySubresourceRegion(mainBuffer.Get(), 0, 0, 0, 0, tempBuffer.Get(), 0, &bbox);*/
			
			
			D3D11_TEXTURE2D_DESC desc;
			tempBuffer.Get()->GetDesc(&desc);
			std::string format = std::to_string(desc.Width);
			std::wstring stemp = std::wstring(format.begin(), format.end());
			LPCWSTR sw = stemp.c_str();
			OutputDebugString(sw);

			//D3D11Context->CopyResource(mainBuffer.Get(), tempBuffer.Get());

			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);



			/*DXGIOutDuplication->ReleaseFrame();
			ID3D11Debug* pDebug = nullptr;
			D3D11Device->QueryInterface(__uuidof(ID3D11Debug), (void**)&pDebug);
			if (pDebug) {
				pDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
				pDebug->Release();
			}*/

			DXGIOutDuplication->ReleaseFrame();

		}

	}

}

LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) 
	{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			return 0;
		case WM_SETCURSOR:
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			return true;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
			
	

	



	

