#include "NetVariance.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "OmniRenderer.h"
#include "RendererCore.h"
#include "SessionManager.h"
#include "WinForge.h"
#include "platform/CaptureController.h"
#include <OmniLink.h>

namespace {

static void HandleFrame(std::vector<WinForge*>* Windows, CHAR* Buffer, DWORD BufferSize)
{
    // zeroth window since i'm still implenting multi window creation
    OmniNet::OmniHeader* header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    WinForge* target = Windows->at(header->Target);
    target->SetBufferData(Buffer, BufferSize);
    target->SetRenderEvent();
}

static void HandleCommand(CHAR* Buffer, DWORD BufferSize)
{
    OmniNet::OmniHeader* header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    if (header->Flags == OmniNet::VoidArg) {
        OmniAPI::ExecuteNetCommand(*reinterpret_cast<CoreCommands*>(Buffer));
    } else {

        ByteStreamReader Reader{static_cast<uint32_t>(BufferSize - 3),
                                reinterpret_cast<uint8_t*>(Buffer)};

        OmniNetCommand Payload = OmniNetCommand::Deserialize(Reader);

        OmniCommand command{Payload};

        NetVariantDeserializer(command.Args,
                               command.ArgTypeIndex,
                               std::make_index_sequence<std::variant_size_v<FuncArgTypes>>(),
                               Payload.Args.data(),
                               Payload.Args.size());

        OmniAPI::ExecuteNetCommandWArgs(command);
    }
}

static void HandleInput(CHAR* Buffer)
{
    INPUT* Payload = reinterpret_cast<INPUT*>(Buffer);
    OmniSynth::ProcInput(*Payload);
}

static void
NetworkPacketHandler(CHAR* Buffer, DWORD BufferSize, uint8_t BufferHeader, void* Context)
{
    std::vector<WinForge*>* WinContext = reinterpret_cast<std::vector<WinForge*>*>(Context);

    switch (BufferHeader) {
    case OmniNet::PacketType::ChunkEnd:
        HandleFrame(WinContext, Buffer, BufferSize);
        break;
    case OmniNet::Command: {
        HandleCommand(Buffer, BufferSize);
        break;
    }
    case OmniNet::PacketType::ProcMouse:
    case OmniNet::PacketType::ProcKey: {
        INPUT* Payload = reinterpret_cast<INPUT*>(Buffer);
        OmniSynth::ProcInput(*Payload);
        break;
    }
    }
};

} // namespace

void OmniCore::ScanInstances()
{
    InstanceReg.RefreshInstanceList([this]() -> void { SetEvent(Events[0]); });
}

void OmniCore::ConnectInstance(DeviceMap DeviceID)
{
    ConnectionRequest request{DeviceID, "OMNILINK"};
    SessionMgr.Connect(request,
                       InstanceReg.UserInstance,
                       InstanceReg.ActiveInstances[DeviceID],
                       NetworkPacketHandler,
                       &ActiveWindows);
    OmniCap.AddEdgeCondition(request.DeviceID);
}

void OmniCore::SwapInstanceLayout(int DeviceID1, int DeviceID2)
{
    InstanceReg.SwapInstances(DeviceMap(DeviceID1), DeviceMap(DeviceID2));
}

void OmniCore::CreateStreamLink(WindowCreationData& WindowInfo)
{

    WinForge* NewWindow = new WinForge();
    ActiveWindows.push_back(NewWindow);

    HWND hwnd_cap = NewWindow->CreateWindowAsync(L"Test Window", hInstance, nCmdShow);
}

void OmniLink::ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index = DeviceMap::C0)
{
    switch (FeatureIndex) {
    case FeatureTypes::ScreenLink: {
        WindowCreationData WGC{"Test Window"};

        OmniNetCommand command{};
        command.CommandType = CoreCommandsWArgs::CreateStreamLink;
        command.ArgTypeIndex = 2;

        std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
        command.Args = payload;
        command.ArgArrayLength = payload.size();

        TransmitNetCommand(Index, command, 0, OmniNet::Argonized);

        CaptureCtrl.AddStream(OmniRenderState.Device,
                              OmniRenderState.Context,
                              InstanceReg.ActiveInstances[Index].InstanceSession,
                              Index,
                              CaptureMode::DXGI);

        break;
    }
    case FeatureTypes::WindowLink: {
        WindowCreationData WGC{"Test Window"};

        OmniNetCommand command{};
        command.CommandType = CoreCommandsWArgs::CreateStreamLink;
        command.ArgTypeIndex = 2;

        std::vector<uint8_t> payload = WindowCreationData::Serialize(WGC);
        command.Args = payload;
        command.ArgArrayLength = payload.size();

        TransmitNetCommand(Index, command, 0, OmniNet::Argonized);

        CaptureCtrl.AddStream(OmniRenderState.Device,
                              OmniRenderState.Context,
                              InstanceReg.ActiveInstances[Index].InstanceSession,
                              Index,
                              CaptureMode::WGC);

        break;
    }

    case FeatureTypes::InputLink:
        OmniCap.ToggleEdgeProbe(hwnd);
        if (OmniCap.GetEdgeProbeState()) {
            InputFilter.InvokeInputFilter();
        } else {
            InputFilter.ReleaseInputFilter();
        }
        break;

    case FeatureTypes::AudioLink: {
        WindowCreationData WGC{"Test Window"};
        CreateStreamLink(WGC);

        break;
    }
    }
}

void OmniLink::OmniMain(HINSTANCE hInst, int nCmdS)
{
    OmniAPI::Ignite(*this);

    Events = new HANDLE[6];

    Events[0] = CreateEventW(NULL, FALSE, TRUE, L"PanelRender");
    Events[1] = CreateEventW(NULL, FALSE, FALSE, L"ToggleWGC");
    Events[2] = CreateEventW(NULL, FALSE, FALSE, L"ToggleDDAPI");
    Events[3] = CreateEventW(NULL, FALSE, FALSE, L"InputLink");
    Events[4] = CreateEventW(NULL, FALSE, FALSE, L"ExecuteCommand");
    Events[5] = CreateEventW(NULL, FALSE, FALSE, L"ExecuteCommandWArgs");

    Logger::log("Event Handler Setup Complete");

    // Control Panel Creation
    WinConfig config(L"Controller Window", 1280, 810, L"Nexus", (LPVOID)this);
    hwnd = WindowInit(config, hInstance, nCmdShow, WProc);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    InitTrayIcon(hwnd);

    Logger::log("Panel Registration Complete");

    OmniRenderer Renderer;

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3DDevice D3DDevStruct =
        Renderer.CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);

    HWNDxD3D11 RendererPtrs;
    RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
    RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
    Renderer.RendererInit(hwnd, 1280, 810, RendererPtrs);

    OmniRenderState.Device = RendererPtrs.D3D11Device.Get();
    OmniRenderState.Context = RendererPtrs.D3D11Context.Get();
    OmniRenderState.Swapchain = RendererPtrs.swapchain.Get();
    OmniRenderState.RTV = RendererPtrs.renderTargetView.Get();

    Logger::log("Renderer Initialization Complete");

    GUI = new OmniGUI(*this);
    GUI->SetupImGui(hwnd, OmniRenderState.Device, OmniRenderState.Context, Events);

    InstanceReg.AwaitNewInstances([this]() -> void { SetEvent(Events[0]); });

    /// Input Capture Test Cases ///

    // OmniCap.WindowMoveListener(true);
    // OmniCap.ToggleInputCapture(hwnd, true);

    /// ......................................... ///

    OmniMainLoop();
}

void OmniLink::OmniMainLoop()
{
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

            OmniRenderState.Context->ClearRenderTargetView(OmniRenderState.RTV, clearColor);
            OmniRenderState.Context->OMSetRenderTargets(1, &OmniRenderState.RTV, nullptr);

            GUI->Render();

            OmniRenderState.Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

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

        case WAIT_OBJECT_0 + 5: {
            unsigned int Tail = CommandBurstQWArgs.Tail;
            switch (CommandBurstQWArgs.Queue[Tail].index()) {
            case 0: {
                auto& args = std::get<0>(CommandBurstQWArgs.Queue[Tail]);
                (this->SwapInstanceLayout)(args.index1, args.index2);
                CommandBurstQWArgs.pop();
                break;
            }

            case 1: {
                ConnectionRequest args = std::get<1>(CommandBurstQWArgs.Queue[Tail]);
                (this->ConnectInstance)(args.DeviceID);
                CommandBurstQWArgs.pop();
                break;
            }

            case 2: {
                WindowCreationData args = std::get<2>(CommandBurstQWArgs.Queue[Tail]);
                (this->CreateStreamLink)(args);
                break;
            }
            }
        } break;

        case WAIT_TIMEOUT:

            (this->*ExecuteCommand)();

            break;
        }
    }
}

void OmniLink::InitTrayIcon(HWND hwnd)
{
    TrayIconData.cbSize = sizeof(NOTIFYICONDATAW);
    TrayIconData.hWnd = hwnd;
    TrayIconData.uID = 62485;
    TrayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    TrayIconData.uCallbackMessage = WM_TRAYICON;
    TrayIconData.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(OmniIcon));
    lstrcpyW(TrayIconData.szTip, L"OmniLink");

    Shell_NotifyIcon(NIM_ADD, &TrayIconData);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

LRESULT CALLBACK OmniLink::WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    OmniLink* omni = reinterpret_cast<OmniLink*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg) {
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

void staging_texture_for_compression(ID3D11Device* D3D11Device,
                                     ID3D11Texture2D* stagingTexture,
                                     UINT width,
                                     UINT height,
                                     DXGI_FORMAT Format)
{
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

void lz4_compression(ID3D11DeviceContext* D3D11Context,
                     ID3D11Texture2D* stagingTexture,
                     ID3D11Texture2D* mainBuffer,
                     unsigned int Width,
                     unsigned int Height)
{
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
            OutputDebugStringW(L"FAILED");
        } else {
            OutputDebugStringW((std::to_wstring(compressedSize) + L"aaa\n").c_str());
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
            OutputDebugStringW(L"FAILED");
        } else {
            OutputDebugStringW((std::to_wstring(compressedSize) + L"bbb\n").c_str());
        }
    });

    t1.join();
    t2.join();
}
