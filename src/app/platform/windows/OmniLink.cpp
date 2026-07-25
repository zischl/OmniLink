#include "D3D11Renderer.h"
#include "NetVariance.h"
#include "OmniDiscovery.h"
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "WinForge.h"
#include <OmniLink.h>
#include <memory>

static void HandleFrame(std::vector<StreamWindow*>* Windows, CHAR* Buffer, DWORD BufferSize)
{
    OmniNet::OmniHeader* Header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    StreamWindow* Target = Windows->at(Header->Target);

    Target->SetBufferData(Buffer, BufferSize - OmniHeaderSize);
    Target->SetRenderEvent();
}

static void HandleCommand(CHAR* Buffer, DWORD BufferSize, DeviceMap DeviceID)
{
    OmniNet::OmniHeader* Header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    if (Header->Flags == OmniNet::VoidArg) {
        OmniAPI::ExecuteNetCommand(*reinterpret_cast<CoreCommands*>(Buffer));
    } else {

        ByteStreamReader Reader{
            static_cast<uint32_t>(BufferSize - 3), reinterpret_cast<uint8_t*>(Buffer)
        };

        OmniNetCommand Payload = OmniNetCommand::Deserialize(Reader);

        if (!OmniAPI::VerifyCommandToken(DeviceID, Payload)) {
            Logger::log(
                "Unauthorized command {:d} received from device {:d} with invalid ActionToken "
                "{:x}, EXTERMINATED!",
                static_cast<int>(Payload.CommandType),
                static_cast<int>(DeviceID),
                Payload.ActionToken
            );
            return;
        }

        OmniCommand command{Payload};

        NetVariantDeserializer(
            command.Args,
            command.ArgTypeIndex,
            std::make_index_sequence<std::variant_size_v<FuncArgTypes>>(),
            Payload.Args.data(),
            Payload.Args.size()
        );

        OmniAPI::ExecuteNetCommandWArgs(command);
    }
}

static void HandleInput(CHAR* Buffer)
{
    INPUT* Payload = reinterpret_cast<INPUT*>(Buffer);
    OmniSynth::ProcInput(*Payload);
}

void NetworkPacketHandler(char* Buffer, uint32_t BufferSize, uint8_t BufferHeader, void* Context)
{
    OmniNet::SessionPacketContext* SessionCtx =
        reinterpret_cast<OmniNet::SessionPacketContext*>(Context);
    std::vector<StreamWindow*>* WindowContext =
        reinterpret_cast<std::vector<StreamWindow*>*>(SessionCtx->UserContext);
    DeviceMap DeviceID = static_cast<DeviceMap>(SessionCtx->UniqueKey);

    switch (BufferHeader) {
    case OmniNet::PacketType::ChunkEnd:
        HandleFrame(WindowContext, Buffer, BufferSize);
        break;
    case OmniNet::Command: {
        HandleCommand(Buffer, BufferSize, DeviceID);
        break;
    }
    case OmniNet::PacketType::ProcMouse:
    case OmniNet::PacketType::ProcKey: {
        HandleInput(Buffer);
        break;
    }
    }
};

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

    GUI = std::make_unique<OmniGUI>(*this);
    GUI->SetupImGui(hwnd, RenderState.Device, RenderState.Context);

    Logger::log("GUI Initialization Complete");

    // Setting up UI Updates on event, Note this ain't the callback given to OmniDiscovery
    // This is da callback for the InstanceRegistery Await, which then combines with RefreshList
    // Before sending it inside OmniDiscovery. so.. technically.. ig it is given to OmniDiscovery
    InstanceRegistry.AwaitNewInstances([this](ProbeEvent Event = {}) -> void {
        UIState = OmniGUIState::RENDER;
        DiscoveryPacketHandler(Event);
    });

    Logger::log("Instance Discovery Initialization Complete");

    SystemLink.SetupSystemLink(hInstance, nCmdShow, hwnd);

    /// Input Capture Test Cases ///

    // OmniCap.WindowMoveListener(true);
    // OmniCap.ToggleInputCapture(hwnd, true);

    /// ......................................... ///

    RunCommandQueue();

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

        if (IsWindowVisible(hwnd) && !IsIconic(hwnd) &&
            (RenderEvent || UIState == OmniGUIState::RENDER)) {
            auto CurrentTime = std::chrono::steady_clock::now();

            if (CurrentTime - LastFrameTime >= FrameTimeLimit) {
                GUI->FrameBegin();

                RenderState.Context->ClearRenderTargetView(RenderState.RTV, clearColor);
                RenderState.Context->OMSetRenderTargets(1, &RenderState.RTV, nullptr);

                GUI->Render();

                RenderState.Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

                LastFrameTime = CurrentTime;
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

void OmniLink::PushNotification(const Notification& notification)
{
    if (GUI)
        GUI->PushNotification(notification);
}

void OmniLink::PushNotification(DeviceMap DeviceID, const Notification& notification)
{
    if (notification.Cancelled) {
        std::lock_guard<std::mutex> lock(EventTokensMutex);
        ActiveEventTokens[DeviceID] = notification.Cancelled;
    }

    if (GUI)
        GUI->PushNotification(notification);
}

void OmniLink::CancelNotification(DeviceMap DeviceID)
{
    std::lock_guard<std::mutex> lock(EventTokensMutex);
    auto iter = ActiveEventTokens.find(DeviceID);
    if (iter != ActiveEventTokens.end()) {
        if (iter->second) {
            iter->second->store(true, std::memory_order_relaxed);
        }
        ActiveEventTokens.erase(iter);
    }
}

void OmniLink::DragWindow()
{
    ReleaseCapture();
    SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void OmniLink::MinimizeWindow()
{
    ShowWindow(hwnd, SW_MINIMIZE);
}

void OmniLink::HideWindow()
{
    ShowWindow(hwnd, SW_HIDE);
}

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK OmniLink::WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    OmniLink* omni = reinterpret_cast<OmniLink*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(hwnd, SW_SHOW);
            SetActiveWindow(hwnd);
            SetForegroundWindow(hwnd);
        } else if (lParam == WM_RBUTTONUP) {
            POINT CursorPos;
            GetCursorPos(&CursorPos);
            HMENU HMenu = CreatePopupMenu();
            if (HMenu) {
                InsertMenuW(HMenu, -1, MF_BYPOSITION, 1, L"Show");
                InsertMenuW(HMenu, -1, MF_BYPOSITION, 2, L"Exit");

                SetForegroundWindow(hwnd);

                int Selected = TrackPopupMenu(
                    HMenu, TPM_RETURNCMD | TPM_NONOTIFY, CursorPos.x, CursorPos.y, 0, hwnd, NULL
                );
                DestroyMenu(HMenu);

                if (Selected == 1) {
                    ShowWindow(hwnd, SW_SHOW);
                    SetActiveWindow(hwnd);
                    SetForegroundWindow(hwnd);
                } else if (Selected == 2) {
                    PostQuitMessage(0);
                }
            }
        }
        break;
    case WM_DESTROY:
        if (omni) {
            Shell_NotifyIcon(NIM_DELETE, &(omni->TrayIconData));
        }
        PostQuitMessage(0);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return 0;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
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
