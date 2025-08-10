
#include "WinForge.h"

WinForge::WinForge(WNDPROC WindowProc) {
	WProc = WindowProc;
}


HWND WinForge::WindowInit(WinConfig& Config, HINSTANCE& hInstance, int nCmdShow) {
	const wchar_t CLASS_NAME[] = { Config.class_name };

	WNDCLASS wc = {};
	wc.lpfnWndProc = WProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	RegisterClass(&wc);

	HWND hwnd_ = CreateWindowEx(
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


	if (hwnd_ == NULL)
	{
		OutputDebugString(L"Window Creation Failed\n");
		return hwnd_;
	}

	ShowWindow(hwnd_, nCmdShow);

	return hwnd_;

}

HWND WinForge::CreateWindowAsync(wchar_t class_name, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct) {

	std::thread test1([&] {

		//CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		WinConfig config(class_name, 1920, 1080, L'Linker', NULL);
		hwnd = WindowInit(config, hInstance, nCmdShow);

		//###############################################################################//

		OmniRenderer Renderer;

		HWNDxD3D11 RendererPtrs;
		RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
		RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
		Renderer.RendererInit(hwnd, config.wdWidth, config.wdHeight, RendererPtrs);
		D3D11Device = RendererPtrs.D3D11Device.Get();
		D3D11Context = RendererPtrs.D3D11Context.Get();
		ContextMode = D3D11Context->GetType();
		
		swapchain = RendererPtrs.swapchain.Get();
		renderTargetView = RendererPtrs.renderTargetView.Get();

		HWNDxShaders ShaderPtrs = Renderer.ShadersInit(D3D11Device);
		pixelShader = ShaderPtrs.pixelShader.Get();
		vertexShader = ShaderPtrs.vertexShader.Get();
		vertexBuffer = ShaderPtrs.vertexBuffer.Get();
		inputLayout = ShaderPtrs.inputLayout.Get();
		IndexBuffer = ShaderPtrs.IndexBuffer.Get();
		sampler = ShaderPtrs.sampler.Get();

		stride = ShaderPtrs.VertexBufferStride;
		offset = ShaderPtrs.VertexBufferOffset;
		
		Renderer.SetShaders(D3D11Context, &ShaderPtrs);

		srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;


		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = config.wdWidth;
		viewport.Height = config.wdHeight;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D11Context->RSSetViewports(1, &viewport);

		//###############################################################################//

		

		custommainBufferDesc = {};
		custommainBufferDesc.Width = config.wdWidth;
		custommainBufferDesc.Height = config.wdHeight;
		custommainBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		custommainBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		custommainBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		custommainBufferDesc.SampleDesc.Count = 1;
		custommainBufferDesc.SampleDesc.Quality = 0;
		custommainBufferDesc.ArraySize = 1;
		custommainBufferDesc.MipLevels = 1;
		custommainBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;


		D3D11Device->CreateTexture2D(&custommainBufferDesc, nullptr, NvdecBuffer.GetAddressOf());

		NVDec = new NVDecoder(config.wdWidth, config.wdHeight, NvdecBuffer.Get());

		//###############################################################################//

		MainLoop();

		});


	test1.detach();

	return hwnd;

}

void WinForge::MainLoop() {
	while (true) {
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//NVDec->NVDecode(reinterpret_cast<const unsigned char*>(datastream), size);


		hr = D3D11Device->CreateShaderResourceView(NvdecBuffer.Get(), &srvDesc, textureView.GetAddressOf());
		if (FAILED(hr)) {
			_com_error err(hr);
			OutputDebugString(err.ErrorMessage());
			OutputDebugString(L"aaaaaaaaaaaaaaaa\n");
			continue;
		}

		D3D11Context->PSSetShaderResources(0, 1, textureView.GetAddressOf());

		D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
		D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
		D3D11Context->Draw(4, 0);

		swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

		//D3D11Context->FinishCommandList();

		Sleep(limit.load());
	}
}

