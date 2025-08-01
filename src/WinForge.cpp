
#include "WinForge.h"


HWND WinForge::CreateWindowAsync(wchar_t class_name, WNDPROC WProc, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct) {

	std::promise<HWND> hwnd_p;
	std::future<HWND> hwnd_f = hwnd_p.get_future();

	std::thread test1([&] {

		//CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		WinConfig config(class_name, 1920, 1080, L'Linker', NULL);
		HWND hwnd = WindowInit(config, WProc, hInstance, nCmdShow);
		hwnd_p.set_value(hwnd);

		//###############################################################################//

		OmniRenderer Renderer;

		HWNDxD3D11 RendererPtrs;
		RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
		RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
		Renderer.RendererInit(hwnd, config.wdWidth, config.wdHeight, RendererPtrs);
		ID3D11Device* D3D11Device = RendererPtrs.D3D11Device.Get();
		ID3D11DeviceContext* D3D11Context = RendererPtrs.D3D11Context.Get();
		ContextMode = D3D11Context->GetType();
		
		IDXGISwapChain3* swapchain = RendererPtrs.swapchain.Get();
		ID3D11RenderTargetView* renderTargetView = RendererPtrs.renderTargetView.Get();

		HWNDxShaders ShaderPtrs = Renderer.ShadersInit(D3D11Device);
		ID3D11PixelShader* pixelShader = ShaderPtrs.pixelShader.Get();
		ID3D11VertexShader* vertexShader = ShaderPtrs.vertexShader.Get();
		ID3D11Buffer* vertexBuffer = ShaderPtrs.vertexBuffer.Get();
		ID3D11InputLayout* inputLayout = ShaderPtrs.inputLayout.Get();
		ID3D11Buffer* IndexBuffer = ShaderPtrs.IndexBuffer.Get();
		ID3D11SamplerState* sampler = ShaderPtrs.sampler.Get();

		UINT stride = ShaderPtrs.VertexBufferStride;
		UINT offset = ShaderPtrs.VertexBufferOffset;
		
		Renderer.SetShaders(D3D11Context, &ShaderPtrs);

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		ComPtr<ID3D11ShaderResourceView> textureView = nullptr;

		float clearColor[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = config.wdWidth;
		viewport.Height = config.wdHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D11Context->RSSetViewports(1, &viewport);

		//###############################################################################//

		HRESULT hr;

		DXGICapture DXGICapture;
		ComPtr<IDXGIOutputDuplication> DXGIOutDuplication = DXGICapture.InitDXGI(RendererPtrs.D3D11Device);

		ComPtr<ID3D11Texture2D> DXGIBuffer = nullptr;
		ID3D11Texture2D* DXGIBufferPtr = nullptr;

		//###############################################################################//



		MSG msg = { };
		while (true) {
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT)
					break;

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			CaptureDXGI(DXGIOutDuplication.Get(), DXGIBuffer);

			hr = D3D11Device->CreateShaderResourceView(DXGIBuffer.Get(), &srvDesc, textureView.GetAddressOf());
			if (FAILED(hr)) {
				_com_error err(hr);
				OutputDebugString(err.ErrorMessage());
				DXGIOutDuplication->ReleaseFrame();
				OutputDebugString(L"aaaaaaaaaaaaaaaa\n");
				continue;
			}

			D3D11Context->PSSetShaderResources(0, 1, textureView.GetAddressOf());

			D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
			D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
			D3D11Context->Draw(4, 0);

			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

			//D3D11Context->FinishCommandList();

			DXGIOutDuplication->ReleaseFrame();
			
		}

		});


	test1.detach();

	HWND hwnd = hwnd_f.get();

	return hwnd;

}


HWND WinForge::WindowInit(WinConfig& Config, WNDPROC WProc, HINSTANCE& hInstance, int nCmdShow) {
	const wchar_t CLASS_NAME[] = { Config.class_name };

	WNDCLASS wc = {};
	wc.lpfnWndProc = WProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		&Config.Window_Name,
		WS_OVERLAPPEDWINDOW,
		0,
		0,
		Config.wdWidth,
		Config.wdHeight,
		NULL,
		NULL,
		hInstance,
		Config.lParam
	);


	if (hwnd == NULL)
	{
		OutputDebugString(L"Window Creation Failed\n");
		return hwnd;
	}

	ShowWindow(hwnd, nCmdShow);

	return hwnd;

}