#ifndef UNICODE
#define UNICODE
#endif

#include <SessionHandler.h>
#include "renderer.h"
#include <nvenc.h>

#include <Windows.h>
#include <string>
#include <wrl/client.h>
#include <iostream>
#include <fstream>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <dcomp.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dcomp.lib")
#include <vector>
#include <wincodec.h>
#include <comdef.h>
#include <lz4.h>
#include <thread>
#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")
//#include <NvEncoder.h>
//#include <NvEncoderD3D11.h>


void nvenc_output_test(NV_ENC_LOCK_BITSTREAM& NVBitstreamLock, const char* fileName) {

	FILE* outFile = fopen(fileName, "wb");
	if (outFile) {
		fwrite(NVBitstreamLock.bitstreamBufferPtr, 1, NVBitstreamLock.bitstreamSizeInBytes, outFile);
		fclose(outFile);
	}
	else {
		OutputDebugString(L"Failed to open output file\n");
	}
}

void staging_texture_for_compression(ID3D11Device* D3D11Device, ID3D11Texture2D* stagingTexture, UINT width, UINT height, DXGI_FORMAT Format) {

	D3D11_TEXTURE2D_DESC stagingBufferDesc = {};
	stagingBufferDesc.Width = width;
	stagingBufferDesc.Height = height;
	stagingBufferDesc.Format = Format;
	stagingBufferDesc.Usage = D3D11_USAGE_STAGING;
	stagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingBufferDesc.BindFlags = 0;
	stagingBufferDesc.SampleDesc.Count = 1;
	stagingBufferDesc.SampleDesc.Quality = 0;
	stagingBufferDesc.ArraySize = 1;
	stagingBufferDesc.MipLevels = 1;
	stagingBufferDesc.MiscFlags = 0;

	D3D11Device->CreateTexture2D(&stagingBufferDesc, nullptr, &stagingTexture);
}

void lz4_compression(ID3D11DeviceContext* D3D11Context, ID3D11Texture2D* stagingTexture, ID3D11Texture2D* mainBuffer, unsigned int Width, unsigned int Height) {

	D3D11Context->CopyResource(stagingTexture, mainBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	D3D11Context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
	BYTE* pixelResource = static_cast<BYTE*>(mappedResource.pData);
	UINT rowPitch = mappedResource.RowPitch;
	D3D11Context->Unmap(stagingTexture, 0);


	UINT TotalChunkedBytes = (rowPitch * Height) / 2;

	std::thread t1([pixelResource, rowPitch, TotalChunkedBytes] {
		std::vector<BYTE> chunk1(TotalChunkedBytes);
		memcpy(chunk1.data(), pixelResource + (0 * rowPitch), TotalChunkedBytes);
		int maxCompressedSize = LZ4_compressBound(TotalChunkedBytes);
		std::vector<char> compressedBuffer(maxCompressedSize);
		int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(chunk1.data()),
			compressedBuffer.data(),
			TotalChunkedBytes,
			maxCompressedSize);
		if (compressedSize <= 0) {
			OutputDebugString(L"FAILED");
		}
		else {
			OutputDebugString((std::to_wstring(compressedSize) + L"aaa\n").c_str());
		}
		});

	std::thread t2([pixelResource, Height, rowPitch, TotalChunkedBytes] {
		std::vector<BYTE> chunk1(TotalChunkedBytes);
		memcpy(chunk1.data(), pixelResource + ((Height / 2) * rowPitch), TotalChunkedBytes);
		int maxCompressedSize = LZ4_compressBound(TotalChunkedBytes);
		std::vector<char> compressedBuffer(maxCompressedSize);
		int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(chunk1.data()),
			compressedBuffer.data(),
			TotalChunkedBytes,
			maxCompressedSize);
		if (compressedSize <= 0) {
			OutputDebugString(L"FAILED");
		}
		else {
			OutputDebugString((std::to_wstring(compressedSize) + L"bbb\n").c_str());
		}
		});


	t1.join();
	t2.join();
}



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
	ComPtr<IDXGISwapChain> swapchain = Renderer.getSwapchain();
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

	///* ################################################################ */

	sessions sessions;
	sessions._init_winsock();
	sockaddr_in address = sessions._create_address("192.168.1.7", 62485);
	SOCKET socketR = sessions._create_socket();
	int packetSize = 1920 * 1080 * 4;
	const char* buffer = "bleh";

	///* ################################################################ */


	ComPtr<ID3D11Texture2D> tempBuffer;
	ComPtr<ID3D11Texture2D> NvencBuffer;
	D3D11_TEXTURE2D_DESC custommainBufferDesc = {};
	custommainBufferDesc.Width = wdWidth;
	custommainBufferDesc.Height = wdHeight;
	custommainBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	custommainBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	custommainBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	custommainBufferDesc.SampleDesc.Count = 1;
	custommainBufferDesc.SampleDesc.Quality = 0;
	custommainBufferDesc.ArraySize = 1;
	custommainBufferDesc.MipLevels = 1;
	custommainBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;


	D3D11Device->CreateTexture2D(&custommainBufferDesc, nullptr, NvencBuffer.GetAddressOf());






	hr = D3D11Device->CreateRenderTargetView(mainBuffer.Get(), nullptr, &renderTargetView);
	if (FAILED(hr)) {
		OutputDebugString(L"RTV Creation Failed. \n");
	}



	

	/*uint32_t NvencGUIDCount;
	NVFunctions.nvEncGetEncodeGUIDCount(NVEncoder, &NvencGUIDCount);
	std::vector<GUID> NvencGUIDs(NvencGUIDCount);
	status = NVFunctions.nvEncGetEncodeGUIDs(NVEncoder, NvencGUIDs.data(), NvencGUIDCount, &NvencGUIDCount);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugString(L"RIP Encode GUIDS \n");
	}
	for (GUID guid : NvencGUIDs) {
		OutputDebugString((L"Supported format: " + std::to_wstring(guid.Data1) + L"\n").c_str());
		OutputDebugString((L"Supported format: " + std::to_wstring(guid.Data2) + L"\n").c_str());
		OutputDebugString((L"Supported format: " + std::to_wstring(guid.Data3) + L"\n").c_str());
		OutputDebugString((L"Supported format: " + std::to_wstring(guid.Data4[0]) + L"\n").c_str());
		OutputDebugString((L"Supported format: " + std::to_wstring(guid.Data4[1]) + L"\n").c_str());
		OutputDebugString((L"Supported format: " + std::to_wstring(guid.Data4[2]) + L"\n").c_str());
	}*/

	/*uint32_t NvencPresetCount;
	NVFunctions.nvEncGetEncodePresetCount(NVEncoder, , &NvencPresetCount);
	GUID NVPresetGUIDs;
	status = NVFunctions.nvEncGetEncodePresetGUIDs(NVEncoder, NvencEncodeGUID, &NVPresetGUIDs, NvencPresetCount, &NvencPresetCount);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugString(L"RIP Encode Preset GUIDS \n");
	}*/



	/*uint32_t NvenvProfileGUIDCount;
	GUID NvProfileGUIDs;
	NVFunctions.nvEncGetEncodeProfileGUIDCount(NVEncoder, NvencEncodeGUID, &NvenvProfileGUIDCount);
	status = NVFunctions.nvEncGetEncodeProfileGUIDs(NVEncoder, NvencEncodeGUID, &NvProfileGUIDs, NvenvProfileGUIDCount, &NvenvProfileGUIDCount);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugString((L"RIP Encode Profile GUID \n" + std::to_wstring(status)).c_str());
	}*/

	/*uint32_t NvenvInputFormatCount = 0;
	status = NVFunctions.nvEncGetInputFormatCount(NVEncoder, NvencEncodeGUID, &NvenvInputFormatCount);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugString((L"RIP Encode Input Format Count \n" + std::to_wstring(status)).c_str());
	}

	std::vector<NV_ENC_BUFFER_FORMAT> NvBufferFormats(NvenvInputFormatCount);
	status = NVFunctions.nvEncGetInputFormats(NVEncoder, NvencEncodeGUID, NvBufferFormats.data(), NvenvInputFormatCount, &NvenvInputFormatCount);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugString((L"RIP Encode Input Formats \n" + std::to_wstring(status)).c_str());
	}*/

	/*for (auto fmt : NvBufferFormats) {
		OutputDebugString((L"Supported Input format: " + std::to_wstring(fmt) + L"\n").c_str());
	}*/
	


	NVENCSTATUS status;
	NVENCODER Nv((void*) D3D11Device.Get(), NvencBuffer.Get(), wdWidth, wdHeight);

	auto NVFunctions = Nv.NVFunctions;
	auto NVEncoder = Nv.NVEncoder;
	NV_ENC_OUTPUT_PTR NvencOutput = Nv.getBitstream();
	NV_ENC_REGISTER_RESOURCE NVRegisterResource = Nv.getRegisteredResource();

	/*##############################################################*/

	ComPtr<ID3D11Texture2D> PlaneYTexture;
	ComPtr<ID3D11Texture2D> PlaneUVTexture;

	D3D11_TEXTURE2D_DESC YTextureDesc = {};
	YTextureDesc.Width = wdWidth;
	YTextureDesc.Height = wdHeight;
	YTextureDesc.Format = DXGI_FORMAT_R8_UNORM;
	YTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	YTextureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	YTextureDesc.SampleDesc.Count = 1;
	YTextureDesc.ArraySize = 1;
	YTextureDesc.MipLevels = 1;

	D3D11_TEXTURE2D_DESC UVTextureDesc = {};
	UVTextureDesc.Width = wdWidth / 2;
	UVTextureDesc.Height = wdHeight / 2;
	UVTextureDesc.Format = DXGI_FORMAT_R8G8_UNORM;
	UVTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	UVTextureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	UVTextureDesc.SampleDesc.Count = 1;
	UVTextureDesc.ArraySize = 1;
	UVTextureDesc.MipLevels = 1;

	D3D11Device->CreateTexture2D(&YTextureDesc, nullptr, PlaneYTexture.GetAddressOf());
	D3D11Device->CreateTexture2D(&UVTextureDesc, nullptr, PlaneUVTexture.GetAddressOf());

	ComPtr<ID3D11UnorderedAccessView> UAViewY;
	ComPtr<ID3D11UnorderedAccessView> UAViewUV;

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAViewYDesc = {};
	UAViewYDesc.Format = YTextureDesc.Format;
	UAViewYDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	UAViewYDesc.Texture2D.MipSlice = 0;
	D3D11Device->CreateUnorderedAccessView(PlaneYTexture.Get(), &UAViewYDesc, UAViewY.GetAddressOf());

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAViewUVDesc = {};
	UAViewUVDesc.Format = UVTextureDesc.Format;
	UAViewUVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	UAViewUVDesc.Texture2D.MipSlice = 0;
	D3D11Device->CreateUnorderedAccessView(PlaneUVTexture.Get(), &UAViewUVDesc, UAViewUV.GetAddressOf());

	ComPtr<ID3D11ComputeShader> NV12ComputeShader;
	ComPtr<ID3DBlob> computeShaderBlob = nullptr;

	hr = D3DReadFileToBlob(L"BGRA2NV12Shader.cso", &computeShaderBlob);
	if (FAILED(hr)) {
		OutputDebugString(L"\nBGRA2NV12Shader Read File Failed Miserably");
	}
	hr = D3D11Device->CreateComputeShader(computeShaderBlob->GetBufferPointer(), computeShaderBlob->GetBufferSize(), nullptr, &NV12ComputeShader);
	if (FAILED(hr)) {
		OutputDebugString(L"\nCompute Shader Creation Failed Miserably");
	}
	D3D11Context->CSSetShader(NV12ComputeShader.Get(), nullptr, 0);

	D3D11_BOX NV12BoxY = {};
	NV12BoxY.top = 0;
	NV12BoxY.left = 0;
	NV12BoxY.front = 0;
	NV12BoxY.bottom = wdHeight;
	NV12BoxY.right = wdWidth;
	NV12BoxY.back = 1;

	D3D11_BOX NV12BoxUV = {};
	NV12BoxUV.top = 0;
	NV12BoxUV.left = 0;
	NV12BoxUV.front = 0;
	NV12BoxUV.bottom = wdHeight / 2;
	NV12BoxUV.right = wdWidth / 2;
	NV12BoxUV.back = 1;



	//##########################################################################################//

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


		hr = DXGIOutDuplication->AcquireNextFrame(500, &frameinfo, &framepixeldata);
		if (SUCCEEDED(hr)) {
			framepixeldata.As(&tempBuffer);

			//#############################################################################################//

			D3D11Context->CopyResource(NvencBuffer.Get(), tempBuffer.Get());

			Nv.Encode();
			

			/*nvenc_output_test(NVBitstreamLock, "bleh1.h264");
			nvenc_output_test(NVBitstreamLock, "bleh2.h264");
			nvenc_output_test(NVBitstreamLock, "bleh3.h264");
			nvenc_output_test(NVBitstreamLock, "bleh4.h264");*/
			

			///* ################################################################ */


			D3D11Context->ClearRenderTargetView(renderTargetView.Get(), clearColor);

			D3D11_VIEWPORT viewport = {};
			viewport.TopLeftX = 0.0f;
			viewport.TopLeftY = 0.0f;
			viewport.Width = wdWidth;
			viewport.Height = wdHeight;
			viewport.MinDepth = 0.0f;
			viewport.MaxDepth = 1.0f;

			D3D11Context->RSSetViewports(1, &viewport);


			hr = D3D11Device->CreateShaderResourceView(tempBuffer.Get(), &srvDesc, textureView.GetAddressOf());
			if (FAILED(hr)) {
				_com_error err(hr);
				OutputDebugString(err.ErrorMessage());
				DXGIOutDuplication->ReleaseFrame();
				OutputDebugString(L"aaaaaaaaaaaaaaaa\n");
				continue;
			}

			//#########################################################################################//

			D3D11Context->CSSetShaderResources(0, 1, textureView.GetAddressOf());
			ID3D11UnorderedAccessView* UAViewsYUV[] = { UAViewY.Get(), UAViewUV.Get() };
			D3D11Context->CSSetUnorderedAccessViews(0, 2, UAViewsYUV, nullptr);

			UINT threadGroupX = (wdWidth + 15) / 16;
			UINT threadGroupY = (wdHeight + 15) / 16;
			D3D11Context->Dispatch(threadGroupX, threadGroupY, 1);

			ID3D11ShaderResourceView* CleanupSRV[1] = { nullptr };
			D3D11Context->CSSetShaderResources(0, 1, CleanupSRV);

			ID3D11UnorderedAccessView* CleanupUAV[2] = { nullptr, nullptr };
			D3D11Context->CSSetUnorderedAccessViews(0, 2, CleanupUAV, nullptr);

			//########################################################################################################//

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

			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

			DXGIOutDuplication->ReleaseFrame();

		}

	}

	//NVFunctions.nvEncDestroyInputBuffer(NVEncoder, NVRegisterResource.registeredResource);
	//NVFunctions.nvEncUnregisterResource(NVEncoder, &NVRegisterResource);
	NVFunctions.nvEncDestroyBitstreamBuffer(NVEncoder, NvencOutput);
	NVFunctions.nvEncDestroyEncoder(NVEncoder);
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



