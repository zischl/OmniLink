
#include "WinForge.h"

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

HWND WindowInit(WinConfig& Config, HINSTANCE hInstance, int nCmdShow, WNDPROC WProc)
{

    WNDCLASSEXW wc = {};
    wc.lpfnWndProc = WProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = Config.class_name.c_str();
    wc.cbSize = sizeof(WNDCLASSEXW);

    ATOM WCAtom = RegisterClassExW(&wc);

    const int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    const int x = (ScreenWidth - Config.wdWidth) / 2;
    const int y = (ScreenHeight - Config.wdHeight) / 2;

    HWND hwnd_ = CreateWindowExW(
        WS_EX_LAYERED,
        MAKEINTATOM(WCAtom),
        Config.Window_Name.c_str(),
        WS_POPUP,
        x,
        y,
        Config.wdWidth,
        Config.wdHeight,
        nullptr,
        NULL,
        hInstance,
        Config.lParam
    );

    // SetLayeredWindowAttributes(hwnd_, RGB(0,0,0), 0, ULW_COLORKEY);

    SetProcessDPIAware();

    if (hwnd_ == NULL) {
        OutputDebugString(L"Window Creation Failed\n");
        OutputDebugString((std::to_wstring(GetLastError()) + L"\n").c_str());
        return hwnd_;
    }

    return hwnd_;
}

HWND WinForge::CreateWindowAsync(
    const wchar_t* window_name, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct
)
{

    std::wstring name(window_name);
    WindowThread = std::thread([this, name, hInstance, nCmdShow, D3DDevStruct] {
        hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        WinConfig config(L"Linker", 1920, 1080, name.c_str(), NULL);
        hwnd = WindowInit(config, hInstance, nCmdShow, WProc);
        ShowWindow(hwnd, nCmdShow);

        Events = new HANDLE[1];
        Events[0] = CreateEvent(NULL, FALSE, TRUE, L"OM_RENDER");

        // ###############################################################################//

        D3D11Renderer Renderer;

        HWNDxD3D11 RendererPtrs;
        RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
        RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
        Renderer.RendererInit(hwnd, config.wdWidth, config.wdHeight, RendererPtrs);
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

        SrvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SrvDesc.Texture2D.MostDetailedMip = 0;
        SrvDesc.Texture2D.MipLevels = 1;

        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = 0.0f;
        viewport.TopLeftY = 0.0f;
        viewport.Width = config.wdWidth;
        viewport.Height = config.wdHeight;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        D3D11Context->RSSetViewports(1, &viewport);

        // ###############################################################################//

        CustommainBufferDesc = {};
        CustommainBufferDesc.Width = config.wdWidth;
        CustommainBufferDesc.Height = config.wdHeight;
        CustommainBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        CustommainBufferDesc.Usage = D3D11_USAGE_DEFAULT;
        CustommainBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        CustommainBufferDesc.SampleDesc.Count = 1;
        CustommainBufferDesc.SampleDesc.Quality = 0;
        CustommainBufferDesc.ArraySize = 1;
        CustommainBufferDesc.MipLevels = 1;
        CustommainBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

        D3D11Device->CreateTexture2D(&CustommainBufferDesc, nullptr, FrameBufferTex.GetAddressOf());

        D3D11Device->CreateShaderResourceView(
            FrameBufferTex.Get(), &SrvDesc, TextureView.GetAddressOf()
        );

        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        OmniDecoder.emplace<NvdecSession>(config.wdWidth, config.wdHeight, FrameBufferTex.Get());

        MainLoop();

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

        case WAIT_OBJECT_0 + 0:
            if (std::chrono::steady_clock::now() - LastFrameTime >= FrameTimeLimit) {
                if (ActiveDecoder != nullptr) [[unlikely]] {
                    ActiveDecoder->Decode(
                        reinterpret_cast<const unsigned char*>(FramePool[CurrentFrame].FrameBuffer),
                        FramePool[CurrentFrame].FrameSize
                    );
                }

                CurrentFrame = (CurrentFrame + 1) & 3;
                Render();

                LastFrameTime = std::chrono::steady_clock::now();
                continue;
            }

            break;

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

LRESULT CALLBACK WinForge::WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // OutputDebugString((L"MSG: " + std::to_wstring(uMsg) + L"\n").c_str());

    switch (uMsg) {
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
