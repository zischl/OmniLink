#include <OmniLink.h>


void OmniCore::WinGetUserName(char(&CharArray)[UNLEN + 1])
{
	TCHAR UserName[UNLEN + 1];
	DWORD size = UNLEN + 1;

	GetUserName((TCHAR*)UserName, &size);

	TCharCpy(*UserName, *CharArray, size);

}

void OmniCore::WinGetComputerName(char(&CharArray)[OmniDevNameLen + 1])
{
	TCHAR ComputerName[OmniDevNameLen + 1];
	DWORD size = sizeof(ComputerName) / sizeof(ComputerName[0]);
	GetComputerName(ComputerName, &size);

	TCharCpy(*ComputerName, *CharArray, size);

}




void OmniCore::QueryLocalIP(uint32_t& LocalIP, const int index)
{

	std::vector<sockaddr_in> LocalIPs;
	sessions::GetLocals(4, &LocalIPs);
	if (!LocalIPs.empty())
	{
		LocalIP = htonl(LocalIPs[index].sin_addr.S_un.S_addr);
		return;
	}

	Logger::log("Failed to Retrieve Local IP : Please Check Your Connection!\n");

}


std::unordered_map<DeviceMap, OmniInstance>* OmniCore::GetAvailableInstances() noexcept { return &AllInstances; }


void OmniCore::SwapInstanceLayout(int source, int dest) {
	if (AllInstances[DeviceMap(dest)].InstanceIP == NULL)
	{
		AllInstances[DeviceMap(dest)].InstanceIP = AllInstances[DeviceMap(source)].InstanceIP;
		strncpy(AllInstances[DeviceMap(dest)].InstanceName, AllInstances[DeviceMap(source)].InstanceName, OmniDevNameLen);
		strncpy(AllInstances[DeviceMap(dest)].IPv4_String, AllInstances[DeviceMap(source)].IPv4_String, 16);

		AllInstances[DeviceMap(source)].Clear();

	}

	else
	{
		OmniInstance temp = AllInstances[DeviceMap(source)];
		AllInstances[DeviceMap(source)].Edit(AllInstances[DeviceMap(dest)].InstanceName, AllInstances[DeviceMap(dest)].IPv4_String, AllInstances[DeviceMap(dest)].InstanceIP);
		AllInstances[DeviceMap(dest)].Edit(temp.InstanceName, temp.IPv4_String, temp.InstanceIP);
	}
}


//Check whether new scan results are available and get them if so
//Otherwise initiate a new scan
void OmniCore::ScanInstances() {
	if (InstanceProbe->ScanState.load()) {
		std::unordered_map<uint32_t, std::string> AvailableInstances = *InstanceProbe->get();

		int DevIdx = 0;

		//AllInstances.clear();

		for (const auto& [IP, Name] : AvailableInstances)
		{
			if (InstanceLookup.contains(IP)) continue;

			std::lock_guard<std::mutex> lock(Mutex);
			AllInstances[static_cast<DeviceMap>(DevIdx)].InstanceIP = IP;
			std::sprintf(AllInstances [static_cast<DeviceMap>(DevIdx)].IPv4_String, "%u.%u.%u.%u", (IP >> 24) & 0xFF, (IP >> 16) & 0xFF, (IP >> 8) & 0xFF, IP & 0xFF);
			DevIdx++;
		}

		for (auto& [DevMapIDx, Instance] : AllInstances)
		{
			if (AvailableInstances.find(Instance.InstanceIP) == AvailableInstances.end())
			{
				Instance.Clear();
			}
		}

		SetEvent(Events[0]);
	}
	else {
		InstanceProbe->Scan(15);
	}
}




void OmniCore::Connect(char IP[16], char Auth[4])
{
	//ActiveInstances[static_cast<DeviceMap>(2)].InstanceSession = new session(sessions.IOCP, ActiveInstances[Local].IPv4_String, AllInstances[index].IPv4_String, 62485, 1450, ActiveInstances[index]);

}

void OmniCore::ConnectInstance(ConnectionRequest request)
{
	ActiveInstances[request.DeviceID] = OmniActiveInstance(AllInstances[request.DeviceID].InstanceName, AllInstances[request.DeviceID].IPv4_String, AllInstances[request.DeviceID].InstanceIP);
	ActiveInstances[request.DeviceID].InstanceSession = new session(sessions.IOCP, AllInstances[DeviceMap::C0].IPv4_String, AllInstances[request.DeviceID].IPv4_String, OmniPort, MTU, &ActiveWindows);
	Logger::log("Connecting to : ", ActiveInstances[request.DeviceID].InstanceName, "at ", ActiveInstances[request.DeviceID].IPv4_String);
	ActiveInstances[request.DeviceID].InstanceSession->OnIOCompletion = NetworkPacketHandler;

	OmniCap.AddEdgeCondition(request.DeviceID);
}


void OmniCore::CreateStreamLink(WindowCreationData& WindowInfo) {

	WinForge* NewWindow = new WinForge();
	ActiveWindows.push_back(NewWindow);
	HWND hwnd_cap = NewWindow->CreateWindowAsync(L'Test Window', hInstance, nCmdShow);

}


void OmniLink::ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index = DeviceMap::C0)
{
	switch (FeatureIndex)
	{
	case FeatureTypes::ScreenLink:
		ToggleDDAPI();
		break;
	case FeatureTypes::WindowLink:
		ToggleWGC();
		break;
	case FeatureTypes::InputLink:
		OmniCap.ToggleEdgeProbe(hwnd);
		if (OmniCap.GetEdgeProbeState()) 
		{ 
			InputFilter.InvokeInputFilter(); 
		}
		else {
			InputFilter.ReleaseInputFilter();
		}
		break;
	
	case FeatureTypes::AudioLink:
		WindowCreationData WGC{ "Test Window" }; 
		CreateStreamLink(WGC);

		break;
	}
}




void OmniLink::ToggleWGC() {

	if (WGCStatus) {
		if (WGSCapture != nullptr)
			WGSCapture->CloseSession();


		delete Nv;
		delete NvencSessionPtr;
		Nv = nullptr;
		

		WGCStatus = false;

		delete WGSCapture;
		WGSCapture = nullptr;

		//ExecuteCommand = &OmniLink::CommandListEmpty;


	}
	else {
		WindowCreationData WGC{ "Test Window" };
		OmniNetCommand command{};
		command.CommandType = CoreCommandsWArgs::CreateStreamLink;
		command.ArgTypeIndex = 2;
		std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
		command.Args = payload;
		command.ArgArrayLength = payload.size();

		TransmitNetCommand(DeviceMap::L1, command, DeviceMap::L1, OmniNet::Argonized);


		WGSCapture = new WGScreenCapture(D3D11Device, D3D11Context);
		WGSCapture->CreateWGCBuffer(D3D11Device, &WGSCapBuffer);
		WGSCapture->CreateMonitorCapSession(WGSCapBuffer, 1920, 1080);
		WGSCapture->StartSession();
		WGCStatus = true;

		Nv = new NVENCODER();
		NvencSessionPtr = new NvencSession(D3D11Device, Nv->NVFunctions, WGSCapBuffer, 1920, 1080);

		AsynLink.StartSpinThread(OmniLink::WGCapSend, ActiveInstances[DeviceMap::L1].InstanceSession, WGSCapture, NvencSessionPtr);

		//ExecuteCommand = &OmniLink::WGCapSend;

	}


}

void OmniLink::ToggleDDAPI() {
	if (DXGIStatus) {
		if (DXGICap != nullptr) {
			DXGIStatus = false;
			delete DXGICap;
			DXGICap = nullptr;
		}

		delete Nv;
		delete NvencSessionPtr;
		Nv = nullptr;
		NvencSessionPtr = nullptr;
		

		//ExecuteCommand = &OmniLink::CommandListEmpty;

		return;
	}
	else {

		WindowCreationData WGC{ "Test Window" };
		OmniNetCommand command{};
		command.CommandType = CoreCommandsWArgs::CreateStreamLink;
		command.ArgTypeIndex = 2;
		std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
		command.Args = payload;
		command.ArgArrayLength = payload.size();

		TransmitNetCommand(DeviceMap::L1, command, DeviceMap::L1, OmniNet::Argonized);


		DXGICap = new DXGICapture;
		DXGICap->InitDXGI(D3D11Device);
		DXGIBuffer = DXGICap->GetBuffer();
		DXGIStatus = true;

		Nv = new NVENCODER();
		NvencSessionPtr = new NvencSession(D3D11Device, Nv->NVFunctions, DXGIBuffer, 1920, 1080);

		AsynLink.StartSpinThread(OmniLink::DXGICapSend, ActiveInstances[DeviceMap::L1].InstanceSession, DXGICap , NvencSessionPtr);

		

		//ExecuteCommand = &OmniLink::DXGICapSend;
	}


}


OmniCore::OmniCore(const HINSTANCE hInst, const int nCmdS) : hInstance(hInst), nCmdShow(nCmdS) {

}


OmniLink::OmniLink(HINSTANCE hInst, int nCmdShow) : OmniCore(hInst, nCmdShow)
{

}	



void OmniLink::OmniMain(HINSTANCE hInst, int nCmdS) {

	OmniAPI::Ignite(*this);

	Events = new HANDLE[6];
	Events[0] = CreateEvent(NULL, FALSE, TRUE, L"PanelRender");
	Events[1] = CreateEvent(NULL, FALSE, FALSE, L"ToggleWGC");
	Events[2] = CreateEvent(NULL, FALSE, FALSE, L"ToggleDDAPI");
	Events[3] = CreateEvent(NULL, FALSE, FALSE, L"InputLink");
	Events[4] = CreateEvent(NULL, FALSE, FALSE, L"ExecuteCommand");
	Events[5] = CreateEvent(NULL, FALSE, FALSE, L"ExecuteCommandWArgs");

	/*##############################################################*/


	//Control Panel Creation
	WinConfig config(L'Controller Window', 1280, 810, L'Nexus', (LPVOID)this);
	hwnd = WindowInit(config, hInstance, nCmdShow, WProc);
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);
	InitTrayIcon(hwnd);

	OmniRenderer Renderer;

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

	D3DDevice D3DDevStruct = Renderer.CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);

	HWNDxD3D11 RendererPtrs;
	RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
	RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
	Renderer.RendererInit(hwnd, 1280, 810, RendererPtrs);
	D3D11Device = RendererPtrs.D3D11Device.Get();
	D3D11Context = RendererPtrs.D3D11Context.Get();
	swapchain = RendererPtrs.swapchain.Get();
	renderTargetView = RendererPtrs.renderTargetView.Get();


	GUI = new OmniGUI(*this);
	GUI->SetupImGui(hwnd, D3D11Device, D3D11Context, Events);

	/*##############################################################*/

	// Getting user data and initializing user instance
	uint32_t LocalIP;
	QueryLocalIP(LocalIP);
	if (LocalIP != 0) {
		IP2Char(LocalIP, ActiveInstances[DeviceMap::C0].IPv4_String);
	}
	WinGetComputerName(ActiveInstances[DeviceMap::C0].InstanceName);
	//OmniCore::UserInstance = ActiveInstances[DeviceMap::C0];


	/*##############################################################*/


	//Creating an instance scanner object. Passing in local device name plus the IP and then the port to use.
	InstanceProbe = new Instances(ActiveInstances[DeviceMap::C0].InstanceName, LocalIP, 62485);
	InstanceProbe->AwaitInstances([this]() {
		ScanInstances();
		});
	ScanInstances();


	/// Input Capture Test Cases ///

	//OmniCap.WindowMoveListener(true);
	//OmniCap.ToggleInputCapture(hwnd, true);
	

	/// ......................................... ///

	
	OmniMainLoop();

}




void OmniLink::OmniMainLoop() {

	while (true) {

		EventDW = MsgWaitForMultipleObjectsEx(6, Events, 10, QS_ALLINPUT, 0);

		switch (EventDW) {
		case WAIT_OBJECT_0 + 6:
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
			GUI->FrameBegin();

			D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
			D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
			//D3D11Context->Draw(4, 0);

			GUI->Render();
			//
			swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
			LastFrameTime = std::chrono::steady_clock::now();

			break;

		case WAIT_OBJECT_0 + 1:

			break;

		case WAIT_OBJECT_0 + 2:

			break;

		case WAIT_OBJECT_0 + 3:

			break;

		case WAIT_OBJECT_0 + 4:
			while (CommandBurstQ.Tail != CommandBurstQ.Head) {
				(this->*CommandTable[CommandBurstQ.Queue[CommandBurstQ.Tail]])();
				CommandBurstQ.pop();
			}
			break;

		case WAIT_OBJECT_0 + 5:
		{
			unsigned int Tail = CommandBurstQWArgs.Tail;
			switch (CommandBurstQWArgs.Queue[Tail].index()) {
			case 0:
			{
				auto& args = std::get<0>(CommandBurstQWArgs.Queue[Tail]);
				(this->SwapInstanceLayout)(args.index1, args.index2);
				CommandBurstQWArgs.pop();
				break;
			}

			case 1:
			{
				ConnectionRequest args = std::get<1>(CommandBurstQWArgs.Queue[Tail]);
				(this->ConnectInstance)(args);
				CommandBurstQWArgs.pop();
				break;
			}

			case 2:
			{
				WindowCreationData args = std::get<2>(CommandBurstQWArgs.Queue[Tail]);
				(this->CreateStreamLink)(args);
				break;
			}


			}
		}
		break;

		case WAIT_TIMEOUT:

			(this->*ExecuteCommand)();

			break;

		}


	}
}

void OmniLink::InitTrayIcon(HWND hwnd) {

	TrayIconData.cbSize = sizeof(NOTIFYICONDATAW);
	TrayIconData.hWnd = hwnd;
	TrayIconData.uID = 62485;
	TrayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	TrayIconData.uCallbackMessage = WM_TRAYICON;
	TrayIconData.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(OmniIcon));
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
		Shell_NotifyIcon(NIM_DELETE, &(omni->TrayIconData));
		return 0;
	case WM_SETCURSOR:
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return true;
	case WM_INPUT:
		(omni->OmniCap.*(omni->OmniCap.InputProc))(lParam);
		break;
	case WM_NCCREATE:
		omni = static_cast<OmniLink*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(omni));
		break;
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
