#include <OmniLink.h>


void OmniLink::ToggleWGC() {

	if (WGCStatus) {
		if (WGSCapture != nullptr)
		WGSCapture->CloseSession();
		
		Nv->NVUnlockBitStream();
		Nv->NVCleanup();
		delete Nv;

		WGCStatus = false;

		delete WGSCapture;
		//delete WGSCapBuffer;

		ExecuteCommand = &OmniLink::CommandListEmpty;

		
	}
	else {
		WGSCapture = new WGScreenCapture(D3D11Device, D3D11Context);
		WGSCapture->CreateWGCBuffer(D3D11Device, &WGSCapBuffer);
		WGSCapture->CreateMonitorCapSession(WGSCapBuffer, 1920, 1080);
		WGSCapture->StartSession();
		WGCStatus = true;
		ExecuteCommand = &OmniLink::WGCapSend;

		Nv = new NVENCODER((void*)D3D11Device, WGSCapBuffer, 1920, 1080);

	}


}

void OmniLink::ToggleDDAPI() {
	if (DXGIStatus) {
		if (DXGICap != nullptr) {
			DXGIStatus = false;
			DXGIOutDuplication.Reset();
			delete DXGICap;
		}

		Nv->NVUnlockBitStream();
		Nv->NVCleanup();
		delete Nv;


		ExecuteCommand = &OmniLink::CommandListEmpty;

		
	}
	else {
		DXGICap = new DXGICapture;
		DXGIOutDuplication = DXGICap->InitDXGI(D3D11Device);
		DXGIBuffer = DXGICap->GetBuffer();
		DXGIStatus = true;
		ExecuteCommand = &OmniLink::DXGICapSend;

		Nv = new NVENCODER((void*)D3D11Device, DXGIBuffer, 1920, 1080);

	}


}

void OmniLink::OmniMain(HINSTANCE hInstance, int nCmdShow) {
	
	Events = new HANDLE[3];
	Events[0] = CreateEvent(NULL, FALSE, TRUE, L"PanelRender");
	Events[1] = CreateEvent(NULL, FALSE, FALSE, L"ToggleWGC");
	Events[2] = CreateEvent(NULL, FALSE, FALSE, L"ToggleDDAPI");


	WinConfig config(L'Controller Window', 1280, 720, L'Nexus', (LPVOID)this);
	HWND hwnd = WindowInit(config, hInstance, nCmdShow, WProc);
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);


	/*##############################################################*/

	OmniRenderer Renderer;

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	D3DDevice D3DDevStruct = Renderer.CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);

	HWNDxD3D11 RendererPtrs;
	RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
	RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
	Renderer.RendererInit(hwnd, 1280, 720, RendererPtrs);
	D3D11Device = RendererPtrs.D3D11Device.Get();
	D3D11Context = RendererPtrs.D3D11Context.Get();
	swapchain = RendererPtrs.swapchain.Get();
	renderTargetView = RendererPtrs.renderTargetView.Get();
	
	/*##############################################################*/

	/*Link = new WinForge(WProc2);
	HWND hwnd_cap = Link->CreateWindowAsync(L'Test Window', hInstance, nCmdShow);*/
	//Link.SetFPSLimit(60);


	/*##############################################################*/
	
	OmniGUI.SetupImGui(hwnd, D3D11Device, D3D11Context, Events);

	/*##############################################################*/

	//ToggleDDAPI();
	//ToggleDDAPI();

	

	



	///* ################################################################ */

	
	session1 = new session(sessions, "192.168.1.7", 62485, 1450, Link);


	///* ################################################################ */


	/*OmniCap.ToggleWindowCap(true);
	OmniCap.ToggleInputEventCap(hwnd, true);*/
	
	OmniMainLoop();

}


int OmniLink::test2(HINSTANCE hInstance, int nCmdShow) {

	Link = new WinForge(WProc2);
	HWND hwnd_cap = Link->CreateWindowAsync(L'Test Window', hInstance, nCmdShow);

	session1 = new session(sessions, "192.168.1.59", 62485, 1450, Link);


	while (true) {

		Sleep(999);
	}

}


void OmniLink::OmniMainLoop() {

	while (true) {

		EventDW = MsgWaitForMultipleObjectsEx(3, Events, 1, QS_ALLINPUT, 0);

		switch (EventDW) {
		case WAIT_OBJECT_0 + 3:
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

				if (msg.message == WM_QUIT)
					break;

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if (std::chrono::steady_clock::now() - LastFrameTime >= FrameTimeLimit) {
				SetEvent(Events[0]);
			}


			break;

		case WAIT_OBJECT_0 + 0:
			OmniGUI.FrameBegin();

			D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
			D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
			//D3D11Context->Draw(4, 0);

			OmniGUI.Render();
			
			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
			LastFrameTime = std::chrono::steady_clock::now();

			break;

		case WAIT_OBJECT_0 + 1:
			ToggleWGC();
			
			break;

		case WAIT_OBJECT_0 + 2:
			ToggleDDAPI();

			break;

		case WAIT_TIMEOUT:
			
			(this->*ExecuteCommand)();

			break;

		}

	}
}

void OmniLink::PanelRendererSwitch(HWND hwnd) {
	

	NOTIFYICONDATAW TrayIconData = {};

	TrayIconData.cbSize = sizeof(TrayIconData);
	TrayIconData.hWnd = hwnd;
	TrayIconData.uID = 62485;
	TrayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	TrayIconData.uCallbackMessage = WM_TRAYICON;
	TrayIconData.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // or your own icon
	lstrcpy(TrayIconData.szTip, L"OmniLink");

	Shell_NotifyIcon(NIM_ADD, &TrayIconData);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


LRESULT CALLBACK OmniLink::WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {


	OmniLink* omni = reinterpret_cast<OmniLink*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

	if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
		return true;

	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		return 0;
	case WM_CLOSE:
		ShowWindow(hwnd, SW_HIDE);
		PanelRendererSwitch(hwnd);
		return 0;
	case WM_SETCURSOR:
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return true;
	case WM_INPUT:
		//(omni->OmniCap.*(omni->OmniCap.InputProc))(lParam);
		break;
	case WM_NCCREATE:
		omni = static_cast<OmniLink*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(omni));
		break;

	
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK OmniLink::WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	//OutputDebugString((L"MSG: " + std::to_wstring(uMsg) + L"\n").c_str());

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

















void D3D_BGRA2NV12_Compute() {
	/*ComPtr<ID3D11Texture2D> PlaneYTexture;
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
	NV12BoxUV.back = 1;*/

	//loooooop
	//#########################################################################################//

		/*D3D11Context->CSSetShaderResources(0, 1, textureView.GetAddressOf());
		ID3D11UnorderedAccessView* UAViewsYUV[] = { UAViewY.Get(), UAViewUV.Get() };
		D3D11Context->CSSetUnorderedAccessViews(0, 2, UAViewsYUV, nullptr);

		UINT threadGroupX = (wdWidth + 15) / 16;
		UINT threadGroupY = (wdHeight + 15) / 16;
		D3D11Context->Dispatch(threadGroupX, threadGroupY, 1);

		ID3D11ShaderResourceView* CleanupSRV[1] = { nullptr };
		D3D11Context->CSSetShaderResources(0, 1, CleanupSRV);

		ID3D11UnorderedAccessView* CleanupUAV[2] = { nullptr, nullptr };
		D3D11Context->CSSetUnorderedAccessViews(0, 2, CleanupUAV, nullptr);*/

		//########################################################################################################//

}



void GetNVBuffer() {

	//TEMP
	/*ComPtr<ID3D11Texture2D> NvencBuffer;
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



	NVENCODER Nv((void*)D3D11Device, NvencBuffer.Get(), wdWidth, wdHeight);*/

	/*#####################################################################################################*/


	/*ComPtr<ID3D11Texture2D> NvdecBuffer;

	custommainBufferDesc = {};
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


	D3D11Device->CreateTexture2D(&custommainBufferDesc, nullptr, NvdecBuffer.GetAddressOf());

	NVDecoder NVDecoder(wdWidth, wdHeight, NvdecBuffer.Get());*/
}

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

	/*ComPtr<ID3D11Texture2D> StagingTex; //for lz4

	D3D11_TEXTURE2D_DESC stagingBufferDesc = {};
	stagingBufferDesc.Width = wdWidth;
	stagingBufferDesc.Height = wdHeight;
	stagingBufferDesc.Format = DXGI_FORMAT_NV12;
	stagingBufferDesc.Usage = D3D11_USAGE_STAGING;
	stagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingBufferDesc.BindFlags = 0;
	stagingBufferDesc.SampleDesc.Count = 1;
	stagingBufferDesc.SampleDesc.Quality = 0;
	stagingBufferDesc.ArraySize = 1;
	stagingBufferDesc.MipLevels = 1;
	stagingBufferDesc.MiscFlags = 0;

	D3D11Device->CreateTexture2D(&stagingBufferDesc, nullptr, &StagingTex);*/
}

void sending_loop_temp() {
	//D3D11Context->CopyResource(NvencBuffer.Get(), DXGIBuffer.Get());

		//Nv.Encode();

		//OutputDebugString((std::to_wstring(Nv.NVBitstreamLock.bitstreamSizeInBytes) + L"\n").c_str());

		//sessions.ChunkedSend(reinterpret_cast<char*>(Nv.NVBitstreamLock.bitstreamBufferPtr), Nv.NVBitstreamLock.bitstreamSizeInBytes, 20000);

		//NVDecoder.NVDecode(reinterpret_cast<const unsigned char*>(Nv.NVBitstreamLock.bitstreamBufferPtr), Nv.NVBitstreamLock.bitstreamSizeInBytes);

		//Nv.NVUnlockBitStream();
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
