#include "D3D11Renderer.h"
#include "NetVariance.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "SessionManager.h"
#include "WinForge.h"
#include <OmniLink.h>
#include <memory>

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

OmniLink::OmniLink(HINSTANCE hInstance_, int nCmdShow_)
{
    hInstance = hInstance_;
    nCmdShow = nCmdShow_;
}

void OmniLink::OmniMain(HINSTANCE hInst, int nCmdS)
{
    OmniAPI::Ignite(*this);

    Logger::log("Event Handler Setup Complete");

    // Control Panel Creation
    WinConfig config(L"Controller Window", 1280, 810, L"Nexus", (LPVOID)this);
    hwnd = WindowInit(config, hInstance, nCmdShow, WProc);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    InitTrayIcon(hwnd);

    Logger::log("Panel Registration Complete");

    D3D11Renderer Renderer;

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3DDevice D3DDevStruct =
        Renderer.CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);

    HWNDxD3D11 RendererPtrs;
    RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
    RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
    Renderer.RendererInit(hwnd, 1280, 810, RendererPtrs);

    RenderState.Device = RendererPtrs.D3D11Device.Get();
    RenderState.Context = RendererPtrs.D3D11Context.Get();
    RenderState.Swapchain = RendererPtrs.swapchain.Get();
    RenderState.RTV = RendererPtrs.renderTargetView.Get();

    Logger::log("Renderer Initialization Complete");

    GUI = new OmniGUI(*this);
    GUI->SetupImGui(hwnd, RenderState.Device, RenderState.Context);

    InstanceRegistry.AwaitNewInstances([this]() -> void { UIState = OmniGUIState::RENDER; });

    /// Input Capture Test Cases ///

    // OmniCap.WindowMoveListener(true);
    // OmniCap.ToggleInputCapture(hwnd, true);

    /// ......................................... ///

    OmniMainLoop();
}

void OmniLink::OmniMainLoop()
{
    while (true) {
        const DWORD Timeout = UIState == OmniGUIState::RENDER ? FrameTimeLimitW : 200;

        DWORD Event =
            MsgWaitForMultipleObjectsEx(0, nullptr, Timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

        bool RenderEvent = false;

        switch (Event) {
        case WAIT_OBJECT_0: {
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    AppState = OmniAppState::STOPPING;
                    return;
                }

                if ((msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) ||
                    (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) ||
                    msg.message == WM_PAINT) {
                    RenderEvent = true;
                }

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            break;
        }
        case WAIT_TIMEOUT: {
            RenderEvent = true;
            break;
        }
        }

        if (RenderEvent || UIState == OmniGUIState::RENDER) {
            auto currentTime = std::chrono::steady_clock::now();

            if (currentTime - LastFrameTime >= FrameTimeLimit) {
                GUI->FrameBegin();

                RenderState.Context->ClearRenderTargetView(RenderState.RTV, clearColor);
                RenderState.Context->OMSetRenderTargets(1, &RenderState.RTV, nullptr);

                GUI->Render();

                RenderState.Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

                LastFrameTime = currentTime;
            }

            UIState = OmniGUIState::IDLE;
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
        (omni->SystemLink.IOCapture.*(omni->SystemLink.IOCapture.InputProc))(lParam);
        break;
    case WM_NCCREATE:
        omni = static_cast<OmniLink*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(omni));
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
