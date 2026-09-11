#include "WinForge.h"
#include "IOLinkContext.h"
#include "OmniTypes.h"
#include <windowsx.h>

WinForge::WinForge(WNDPROC WindowProc)
{
    WProc = WindowProc;
    SetFramePoolSize(FrameQueueSize);
}

WinForge::~WinForge()
{
    CloseWindowThread();
    CleanupD3D();
    CleanupEvents();
    CleanupFramePool();
}

HWND WindowInit(const WinConfig& Config, HINSTANCE hInstance, int nCmdShow, WNDPROC WProc)
{
    WNDCLASSEXW WndClassExW = {};
    if (!GetClassInfoExW(hInstance, Config.ClassName, &WndClassExW)) {
        WndClassExW               = {};
        WndClassExW.cbSize        = sizeof(WNDCLASSEXW);
        WndClassExW.lpfnWndProc   = WProc;
        WndClassExW.hInstance     = hInstance;
        WndClassExW.lpszClassName = Config.ClassName;

        if (RegisterClassExW(&WndClassExW) == 0) {
            OutputDebugString(
                (L"Window Class Reg Died: " + std::to_wstring(GetLastError()) + L"\n").c_str()
            );
        }
    }

    const int ScreenWidth  = GetSystemMetrics(SM_CXSCREEN);
    const int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    const int CenterX = (ScreenWidth - static_cast<int>(Config.wdWidth)) / 2;
    const int CenterY = (ScreenHeight - static_cast<int>(Config.wdHeight)) / 2;

    HWND hwnd_ = CreateWindowExW(
        WS_EX_LAYERED,
        Config.ClassName,
        Config.WindowName,
        WS_POPUP,
        CenterX,
        CenterY,
        Config.wdWidth,
        Config.wdHeight,
        nullptr,
        NULL,
        hInstance,
        Config.lParam
    );

    if (hwnd_ == NULL) {
        OutputDebugString(L"Window Creation Failed\n");
        OutputDebugString((std::to_wstring(GetLastError()) + L"\n").c_str());
        return hwnd_;
    }

    return hwnd_;
}

HWND WinForge::CreateWindowAsync(
    const wchar_t* WindowName,
    HINSTANCE&     hInstance,
    int            nCmdShow,
    uint32_t       Width,
    uint32_t       Height,
    D3DDevice      D3DDevStruct
)
{
    std::wstring name(WindowName ? WindowName : L"Window Link");
    WindowThread = std::thread([this, name, hInstance, nCmdShow, Width, Height, D3DDevStruct] {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        if (Width == 0 || Height == 0) {
            const int ScreenW = GetSystemMetrics(SM_CXSCREEN);
            const int ScreenH = GetSystemMetrics(SM_CYSCREEN);
            WindowWidth       = (Width > 0) ? Width : static_cast<uint32_t>(ScreenW);
            WindowHeight      = (Height > 0) ? Height : static_cast<uint32_t>(ScreenH);
        } else {
            WindowWidth  = Width;
            WindowHeight = Height;
        }

        TextureWidth  = WindowWidth;
        TextureHeight = WindowHeight;

        WinConfig config(L"Linker", WindowWidth, WindowHeight, name.c_str(), this);
        hwnd = WindowInit(config, hInstance, nCmdShow, WProc);
        if (hwnd == NULL) {
            CoUninitialize();
            return;
        }

        Events    = new HANDLE[1];
        Events[0] = CreateEvent(NULL, FALSE, TRUE, NULL);

        // ###############################################################################//

        D3D11Renderer Renderer;

        HWNDxD3D11 RendererPtrs;
        RendererPtrs.D3D11Device  = D3DDevStruct.D3D11Device;
        RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
        Renderer.RendererInit(hwnd, WindowWidth, WindowHeight, RendererPtrs);
        D3D11Device = RendererPtrs.D3D11Device.Get();
        if (D3D11Device)
            D3D11Device->AddRef();

        D3D11Context = RendererPtrs.D3D11Context.Get();
        if (D3D11Context)
            D3D11Context->AddRef();

        ContextMode = D3D11Context ? D3D11Context->GetType() : D3D11_DEVICE_CONTEXT_IMMEDIATE;

        Swapchain = RendererPtrs.swapchain.Get();
        if (Swapchain)
            Swapchain->AddRef();

        RenderTargetView = RendererPtrs.renderTargetView.Get();
        if (RenderTargetView)
            RenderTargetView->AddRef();

        HWNDxShaders ShaderPtrs = Renderer.ShadersInit(D3D11Device);

        PixelShader = ShaderPtrs.pixelShader.Get();
        if (PixelShader)
            PixelShader->AddRef();

        VertexShader = ShaderPtrs.vertexShader.Get();
        if (VertexShader)
            VertexShader->AddRef();

        VertexBuffer = ShaderPtrs.vertexBuffer.Get();
        if (VertexBuffer)
            VertexBuffer->AddRef();

        InputLayout = ShaderPtrs.inputLayout.Get();
        if (InputLayout)
            InputLayout->AddRef();

        IndexBuffer = ShaderPtrs.IndexBuffer.Get();
        if (IndexBuffer)
            IndexBuffer->AddRef();

        Sampler = ShaderPtrs.sampler.Get();
        if (Sampler)
            Sampler->AddRef();

        Stride = ShaderPtrs.VertexBufferStride;
        Offset = ShaderPtrs.VertexBufferOffset;

        Renderer.SetShaders(D3D11Context, &ShaderPtrs);

        SrvDesc.Format                    = DXGI_FORMAT_B8G8R8A8_UNORM;
        SrvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.MipLevels       = 1;

        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX       = 0.0f;
        viewport.TopLeftY       = 0.0f;
        viewport.Width          = config.wdWidth;
        viewport.Height         = config.wdHeight;
        viewport.MinDepth       = 0.0f;
        viewport.MaxDepth       = 1.0f;

        if (D3D11Context) {
            D3D11Context->RSSetViewports(1, &viewport);
        }

        // ###############################################################################//

        CustommainBufferDesc           = {};
        CustommainBufferDesc.Width     = WindowWidth;
        CustommainBufferDesc.Height    = WindowHeight;
        CustommainBufferDesc.Format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        CustommainBufferDesc.Usage     = D3D11_USAGE_DEFAULT;
        CustommainBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        CustommainBufferDesc.SampleDesc.Count   = 1;
        CustommainBufferDesc.SampleDesc.Quality = 0;
        CustommainBufferDesc.ArraySize          = 1;
        CustommainBufferDesc.MipLevels          = 1;
        CustommainBufferDesc.MiscFlags          = D3D11_RESOURCE_MISC_SHARED;

        if (D3D11Device) {
            D3D11Device->CreateTexture2D(
                &CustommainBufferDesc, nullptr, FrameBufferTex.GetAddressOf()
            );

            D3D11Device->CreateShaderResourceView(
                FrameBufferTex.Get(), &SrvDesc, TextureView.GetAddressOf()
            );
        }

        if (nCmdShow != SW_HIDE) {
            ShowWindow(hwnd, nCmdShow);
            UpdateWindow(hwnd);
        }

        OmniDecoder.emplace<NvdecSession>(WindowWidth, WindowHeight, FrameBufferTex.Get());

        MainLoop();

        CleanupD3D();
        CleanupEvents();

        CoUninitialize();
    });

    return hwnd;
}

void WinForge::Render()
{
    D3D11Context->PSSetShaderResources(0, 1, TextureView.GetAddressOf());

    D3D11Context->ClearRenderTargetView(RenderTargetView, ClearColor);
    D3D11Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);
    D3D11Context->Draw(4, 0);

    Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
}

void WinForge::MainLoop()
{
    NvdecSession* ActiveDecoder = std::get_if<NvdecSession>(&OmniDecoder);

    while (true) {
        EventDW = MsgWaitForMultipleObjectsEx(1, Events, 0, QS_ALLINPUT, 0);

        switch (EventDW) {
        case WAIT_OBJECT_0 + 1:
            while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE)) {

                if (Msg.message == WM_QUIT)
                    return;

                if (Msg.message == WM_SWAP_DECODER) {
                    ActiveDecoder = std::get_if<NvdecSession>(&OmniDecoder);
                    continue;
                }

                TranslateMessage(&Msg);
                DispatchMessage(&Msg);
            }

            break;

        case WAIT_OBJECT_0 + 0: {
            uint64_t Decoded = DecodedCount;
            uint64_t Queued  = QueuedCount.load(std::memory_order_acquire);

            while (Decoded < Queued) {
                uint32_t slot = static_cast<uint32_t>(Decoded % FrameQueueSize);
                if (ActiveDecoder != nullptr) [[likely]] {
                    ActiveDecoder->Decode(
                        reinterpret_cast<const unsigned char*>(FramePool[slot].FrameBuffer),
                        FramePool[slot].FrameSize
                    );
                }
                ++Decoded;
            }

            if (DecodedCount != Decoded) {
                DecodedCount = Decoded;
                Render();
                LastFrameTime = std::chrono::steady_clock::now();
            }

            break;
        }

        case WAIT_TIMEOUT:
            break;
        }
    }
}

void WinForge::CloseWindowThread()
{
    if (hwnd != NULL && IsWindow(hwnd)) {
        PostMessage(hwnd, WM_CLOSE, 0, 0);
    }
    if (Events != nullptr && Events[0] != NULL) {
        SetEvent(Events[0]);
    }
    if (WindowThread.joinable()) {
        WindowThread.join();
    }
}

void WinForge::CleanupD3D()
{
    TextureView.Reset();
    FrameBufferTex.Reset();

    if (Sampler) {
        Sampler->Release();
        Sampler = nullptr;
    }
    if (IndexBuffer) {
        IndexBuffer->Release();
        IndexBuffer = nullptr;
    }
    if (InputLayout) {
        InputLayout->Release();
        InputLayout = nullptr;
    }
    if (VertexBuffer) {
        VertexBuffer->Release();
        VertexBuffer = nullptr;
    }
    if (VertexShader) {
        VertexShader->Release();
        VertexShader = nullptr;
    }
    if (PixelShader) {
        PixelShader->Release();
        PixelShader = nullptr;
    }
    if (RenderTargetView) {
        RenderTargetView->Release();
        RenderTargetView = nullptr;
    }
    if (Swapchain) {
        Swapchain->Release();
        Swapchain = nullptr;
    }
    if (D3D11Context) {
        D3D11Context->Release();
        D3D11Context = nullptr;
    }
    if (D3D11Device) {
        D3D11Device->Release();
        D3D11Device = nullptr;
    }
}

void WinForge::CleanupEvents()
{
    if (Events != nullptr) {
        if (Events[0] != NULL) {
            CloseHandle(Events[0]);
            Events[0] = NULL;
        }
        delete[] Events;
        Events = nullptr;
    }
}

// Usual window proc extended with the ability to forward inputs from either mouse or keyboard
// Will later update after adding dynamic keyboard shortcut mappings
LRESULT CALLBACK WinForge::WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

    WinForge* WinForgePtr = reinterpret_cast<WinForge*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (uMsg) {
    case WM_SIZE:
        if (WinForgePtr) {
            WinForgePtr->WindowWidth  = LOWORD(lParam);
            WinForgePtr->WindowHeight = HIWORD(lParam);
        }
        return 0;

    case WM_MOUSEACTIVATE:
        SetFocus(hwnd);
        return MA_ACTIVATE;

    case WM_MOUSEMOVE: {
        if (WinForgePtr && WinForgePtr->GetEventForwardingState()) {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            uint32_t width  = WinForgePtr->WindowWidth > 0 ? WinForgePtr->WindowWidth : 1;
            uint32_t height = WinForgePtr->WindowHeight > 0 ? WinForgePtr->WindowHeight : 1;

            uint16_t normX = static_cast<uint16_t>(
                (static_cast<uint64_t>(std::clamp(x, 0, static_cast<int>(width))) * 65535ULL) /
                width
            );
            uint16_t normY = static_cast<uint16_t>(
                (static_cast<uint64_t>(std::clamp(y, 0, static_cast<int>(height))) * 65535ULL) /
                height
            );

            if (normX != WinForgePtr->LastNormalizedX || normY != WinForgePtr->LastNormalizedY) {
                WinForgePtr->LastNormalizedX = normX;
                WinForgePtr->LastNormalizedY = normY;

                OmniMousePacket Packet = {};
                Packet.dX              = normX;
                Packet.dY              = normY;
                Packet.Flags           = OMNI_MOUSE_ABSOLUTE;

                WinForgePtr->InputHandler(&Packet, sizeof(OmniMousePacket), true);
            }
        }
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP: {
        if (WinForgePtr && WinForgePtr->GetEventForwardingState()) {
            int X = GET_X_LPARAM(lParam);
            int Y = GET_Y_LPARAM(lParam);

            uint32_t Width  = WinForgePtr->WindowWidth > 0 ? WinForgePtr->WindowWidth : 1;
            uint32_t Height = WinForgePtr->WindowHeight > 0 ? WinForgePtr->WindowHeight : 1;

            uint16_t NormalizedX = static_cast<uint16_t>(
                (static_cast<uint64_t>(std::clamp(X, 0, static_cast<int>(Width))) * 65535ULL) /
                Width
            );
            uint16_t NormalizedY = static_cast<uint16_t>(
                (static_cast<uint64_t>(std::clamp(Y, 0, static_cast<int>(Height))) * 65535ULL) /
                Height
            );

            WinForgePtr->LastNormalizedX = NormalizedX;
            WinForgePtr->LastNormalizedY = NormalizedY;

            OmniMousePacket Packet = {};
            Packet.dX              = NormalizedX;
            Packet.dY              = NormalizedY;
            Packet.Flags           = OMNI_MOUSE_ABSOLUTE;

            static constexpr uint16_t MouseButtonEvents[] = {
                MOUSEEVENTF_LEFTDOWN,
                MOUSEEVENTF_LEFTUP,
                0,
                MOUSEEVENTF_RIGHTDOWN,
                MOUSEEVENTF_RIGHTUP,
                0,
                MOUSEEVENTF_MIDDLEDOWN,
                MOUSEEVENTF_MIDDLEUP,
                0,
                0,
                MOUSEEVENTF_XDOWN,
                MOUSEEVENTF_XUP
            };

            Packet.Buttons = MouseButtonEvents[uMsg - WM_LBUTTONDOWN];

            if (uMsg >= WM_XBUTTONDOWN) [[unlikely]] {
                Packet.Wheel = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? XBUTTON1 : XBUTTON2;
            }

            // It may look weird but .. All Down events have a bit in 0x00AA,
            // Up events have a bit in 0x0154
            if (Packet.Buttons & 0x00AA) {
                SetCapture(hwnd);
                SetFocus(hwnd);
            } else {
                constexpr WPARAM MOUSE_BUTTONS_MASK =
                    MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2;
                if ((wParam & MOUSE_BUTTONS_MASK) == 0) {
                    ReleaseCapture();
                }
            }

            WinForgePtr->InputHandler(&Packet, sizeof(OmniMousePacket), true);
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL: {
        if (WinForgePtr && WinForgePtr->GetEventForwardingState()) {
            POINT Point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &Point);

            uint32_t Width  = WinForgePtr->WindowWidth > 0 ? WinForgePtr->WindowWidth : 1;
            uint32_t Height = WinForgePtr->WindowHeight > 0 ? WinForgePtr->WindowHeight : 1;

            uint16_t normX = static_cast<uint16_t>(
                (static_cast<uint64_t>(std::clamp((int)Point.x, 0, static_cast<int>(Width))) *
                 65535ULL) /
                Width
            );
            uint16_t normY = static_cast<uint16_t>(
                (static_cast<uint64_t>(std::clamp((int)Point.y, 0, static_cast<int>(Height))) *
                 65535ULL) /
                Height
            );

            OmniMousePacket Packet = {};
            Packet.dX              = normX;
            Packet.dY              = normY;
            Packet.Flags           = OMNI_MOUSE_ABSOLUTE;
            Packet.Buttons = (uMsg == WM_MOUSEWHEEL) ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL;
            Packet.Wheel   = GET_WHEEL_DELTA_WPARAM(wParam);

            WinForgePtr->InputHandler(&Packet, sizeof(OmniMousePacket), true);
        }
        return 0;
    }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        if (!WinForgePtr)
            break;

        // Emergency breakout toggle: Ctrl + Shift + X
        if ((wParam == 'X') && (GetKeyState(VK_CONTROL) & 0x8000) &&
            (GetKeyState(VK_SHIFT) & 0x8000)) {
            if (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN) {
                WinForgePtr->ToggleEventForwarding();
            }
            return 0;
        }

        if (WinForgePtr->GetEventForwardingState()) {
            OmniKeyPacket KeyPacket = {};
            KeyPacket.VkCode        = static_cast<uint16_t>(wParam);
            KeyPacket.ScanCode      = static_cast<uint16_t>((lParam >> 16) & 0xFF);
            KeyPacket.Flags         = 0;

            if (KeyPacket.ScanCode != 0) {
                KeyPacket.Flags |= KEYEVENTF_SCANCODE;
            }
            if (lParam & (1 << 24)) {
                KeyPacket.Flags |= KEYEVENTF_EXTENDEDKEY;
            }
            if (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP) {
                KeyPacket.Flags |= KEYEVENTF_KEYUP;
            }

            WinForgePtr->InputHandler(&KeyPacket, sizeof(OmniKeyPacket), false);

            // Alt+F4 will proceed to DefWindowProc so that this window can be closed normally
            if (uMsg == WM_SYSKEYDOWN && wParam == VK_F4 && (lParam & (1 << 29))) {
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
            }
            return 0;
        }
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        if (WinForgePtr && WinForgePtr->EventHandler.OnWindowClose) {
            WinForgePtr->EventHandler.OnWindowClose(WinForgePtr->EventHandler.Context);
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return true;
    case WM_NCCREATE:
        auto DataPtr =
            reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, DataPtr);
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
