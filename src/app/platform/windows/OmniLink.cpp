#include "ClipBoardLink.h"
#include "ClipboardTypes.hpp"
#include "D3D11Renderer.hpp"
#include "NetVariance.hpp"
#include "OmniDiscovery.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SystemLink.h"
#include "WinForge.hpp"
#include "WindowOperationTypes.hpp"
#include <OmniLink.hpp>

#include <memory>

static void
HandleFrame(std::unordered_map<SubStreamID, StreamWindow*>* Windows, CHAR* Buffer, DWORD BufferSize)
{
    OmniNet::OmniHeader* Header = reinterpret_cast<OmniNet::OmniHeader*>((Buffer + BufferSize - 3));
    if (Windows) {
        auto IterStreamWindows = Windows->find(Header->Target);
        if (IterStreamWindows != Windows->end() && IterStreamWindows->second) {
            StreamWindow* Target = IterStreamWindows->second;
            Target->SetBufferData(Buffer, BufferSize - OmniHeaderSize);
            Target->SetRenderEvent();
        }
    }
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

        OmniCommand command{Payload, DeviceID};

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

static void HandleMouse(CHAR* Buffer, uint32_t BufferSize)
{
    if (!Buffer || BufferSize < sizeof(OmniMousePacket))
        return;

    const auto* Packet = reinterpret_cast<const OmniMousePacket*>(Buffer);
    OmniSynth::ProcMouse(*Packet);
}

static void HandleKey(CHAR* Buffer, uint32_t BufferSize)
{
    if (!Buffer || BufferSize < sizeof(OmniKeyPacket))
        return;

    const auto* Packet = reinterpret_cast<const OmniKeyPacket*>(Buffer);
    OmniSynth::ProcKey(*Packet);
}

static void HandleBoundary(CHAR* Buffer, uint32_t BufferSize)
{
    if (!Buffer || BufferSize < sizeof(OmniBoundaryPacket))
        return;

    const auto* Packet = reinterpret_cast<const OmniBoundaryPacket*>(Buffer);
    OmniSynth::ProcBoundary(*Packet);
}

static void HandleClipboard(CHAR* Buffer, uint32_t BufferSize)
{
    if (!Buffer || BufferSize <= OmniHeaderSize)
        return;

    uint32_t PayloadSize = BufferSize - OmniHeaderSize;
    if (PayloadSize < 1)
        return;

    uint8_t Op = static_cast<uint8_t>(Buffer[0]);
    if (Op == static_cast<uint8_t>(ClipboardOp::LightGram)) {
        std::string Text(Buffer + 1, PayloadSize - 1);
        OmniClipboardLink::SetClipTypeText(Text);
    } else if (Op == static_cast<uint8_t>(ClipboardOp::Manifest)) {
        ByteStreamReader  Reader{PayloadSize - 1, reinterpret_cast<uint8_t*>(Buffer + 1)};
        ClipboardManifest Manifest = ClipboardManifest::Deserialize(Reader);
        OmniClipboardLink::AddClipItemPromise(Manifest);
    }
}

void NetworkPacketHandler(char* Buffer, uint32_t BufferSize, uint8_t BufferHeader, void* Context)
{
    OmniNet::SessionPacketContext* SessionCtx =
        reinterpret_cast<OmniNet::SessionPacketContext*>(Context);
    DeviceMap DeviceID = static_cast<DeviceMap>(SessionCtx->UniqueKey);

    switch (BufferHeader) {
    case OmniNet::PacketType::ChunkEnd: {
        // This route is now deprecated due to capture streams using SubStreams
        OmniSystemLink* SysLink = reinterpret_cast<OmniSystemLink*>(SessionCtx->UserContext);

        std::unordered_map<SubStreamID, StreamWindow*>* WindowContext =
            SysLink ? &SysLink->StreamWindowRegistry : nullptr;

        HandleFrame(WindowContext, Buffer, BufferSize);
        break;
    }
    case OmniNet::Command: {
        HandleCommand(Buffer, BufferSize, DeviceID);
        break;
    }
    case OmniNet::PacketType::ProcMouse: {
        HandleMouse(Buffer, BufferSize);
        break;
    }
    case OmniNet::PacketType::ProcKey: {
        HandleKey(Buffer, BufferSize);
        break;
    }
    case OmniNet::PacketType::ProcBoundary: {
        HandleBoundary(Buffer, BufferSize);
        break;
    }
    case OmniNet::PacketType::ProcWinDrag: {
        OmniSystemLink* SysLink = reinterpret_cast<OmniSystemLink*>(SessionCtx->UserContext);

        if (BufferSize >= sizeof(OmniWinDragPacket)) {
            SysLink->HandleStreamWindowDrag(
                *reinterpret_cast<const OmniWinDragPacket*>(Buffer), DeviceID
            );
        }
        break;
    }
    case OmniNet::PacketType::ProcWinResize: {
        OmniSystemLink* SysLink = reinterpret_cast<OmniSystemLink*>(SessionCtx->UserContext);

        if (BufferSize >= sizeof(OmniWinResizePacket)) {
            SysLink->HandleStreamWindowResize(
                *reinterpret_cast<const OmniWinResizePacket*>(Buffer)
            );
        }
        break;
    }
    case OmniNet::PacketType::ProcClipboard: {
        HandleClipboard(Buffer, BufferSize);
        break;
    }
    }
};

OmniLink::OmniLink(HINSTANCE hInstance_, int nCmdShow_)
{
    hInstance = hInstance_;
    nCmdShow  = nCmdShow_;
}

void OmniLink::OmniMain(HINSTANCE hInst, int nCmdS)
{
    SetProcessDPIAware();
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
    UINT              creationFlags   = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3DDevice D3DDevStruct =
        Renderer.CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);

    HWNDxD3D11 RendererPtrs;
    RendererPtrs.D3D11Device  = D3DDevStruct.D3D11Device;
    RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
    Renderer.RendererInit(hwnd, 1280, 810, RendererPtrs);

    GraphicsContext.Device    = RendererPtrs.D3D11Device.Get();
    GraphicsContext.Context   = RendererPtrs.D3D11Context.Get();
    GraphicsContext.Swapchain = RendererPtrs.swapchain.Get();
    GraphicsContext.RTV       = RendererPtrs.renderTargetView.Get();

    Logger::log("Renderer Initialization Complete");

    GUI = std::make_unique<OmniGUI>(*this);
    GUI->SetupImGui(hwnd, GraphicsContext.Device, GraphicsContext.Context);

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
    SystemLink.OnClipboardStreamEvent = [this](const ClipboardStreamEvent& Event) {
        Notification Notif{
            Event, "ClipboardStream", Notification::EventLayout::BOTTOM_RIGHT, 30.0f, true, nullptr
        };
        PushNotification(Event.DeviceID, Notif);
    };

    /// ......................................... ///

    RunCommandQueue();

    OmniMainLoop();
}

void OmniLink::OmniMainLoop()
{
    while (true) {
        bool        NotificationsAvailable = GUI && GUI->ActiveNotificationsAvailable();
        const DWORD Timeout =
            (UIState == OmniGUIState::RENDER || NotificationsAvailable) ? FrameTimeLimitW : 200;

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

        if (((IsWindowVisible(hwnd) && !IsIconic(hwnd)) || NotificationsAvailable) &&
            (RenderEvent || UIState == OmniGUIState::RENDER || NotificationsAvailable)) {
            auto CurrentTime = std::chrono::steady_clock::now();

            if (CurrentTime - LastFrameTime >= FrameTimeLimit) {
                GUI->FrameBegin();

                GraphicsContext.Context->ClearRenderTargetView(GraphicsContext.RTV, ClearColor);
                GraphicsContext.Context->OMSetRenderTargets(1, &GraphicsContext.RTV, nullptr);

                GUI->Render();

                GraphicsContext.Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

                LastFrameTime = CurrentTime;
            }

            UIState = OmniGUIState::IDLE;
        }
    }
}

void OmniLink::InitTrayIcon(HWND hwnd)
{
    TrayIconData.cbSize           = sizeof(NOTIFYICONDATAW);
    TrayIconData.hWnd             = hwnd;
    TrayIconData.uID              = 62485;
    TrayIconData.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    TrayIconData.uCallbackMessage = WM_TRAYICON;
    TrayIconData.hIcon            = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(OmniIcon));
    lstrcpyW(TrayIconData.szTip, L"OmniLink");

    Shell_NotifyIconW(NIM_ADD, &TrayIconData);
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
    auto                        iter = ActiveEventTokens.find(DeviceID);
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
    OmniLink* Omni = reinterpret_cast<OmniLink*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg) {
    case WM_CLIPBOARDUPDATE:
        if (Omni) {
            Omni->SystemLink.ClipboardLink.OnClipboardUpdate();
        }
        return 0;
    case WM_RENDERFORMAT:
        if (Omni) {
            Omni->SystemLink.ClipboardLink.OnPasteRequest(static_cast<UINT>(wParam));
        }
        return 0;
    case WM_DESTROYCLIPBOARD:
        if (Omni) {
            Omni->SystemLink.ClipboardLink.OnRequestInvalidation();
        }
        return 0;
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
        if (Omni) {
            Shell_NotifyIconW(NIM_DELETE, &(Omni->TrayIconData));
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
        if (Omni && Omni->SystemLink.InputLink.InputProc != nullptr) {
            (Omni->SystemLink.InputLink.*(Omni->SystemLink.InputLink.InputProc))(lParam);
        }
        break;
    case WM_NCCREATE:
        Omni = static_cast<OmniLink*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Omni));
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
