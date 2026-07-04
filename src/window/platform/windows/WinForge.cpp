
#include "WinForge.h"

WinForge::WinForge(WNDPROC WindowProc) {
  WProc = WindowProc;
  SetFramePoolSize(FrameQueueSize);
}

HWND WindowInit(WinConfig &Config, HINSTANCE hInstance, int nCmdShow,
                WNDPROC WProc) {

  WNDCLASSEXW wc = {};
  wc.lpfnWndProc = WProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = Config.class_name.c_str();
  wc.cbSize = sizeof(WNDCLASSEXW);

  ATOM WCAtom = RegisterClassExW(&wc);

  HWND hwnd_ = CreateWindowExW(WS_EX_LAYERED, MAKEINTATOM(WCAtom),
                               Config.Window_Name.c_str(), WS_POPUP, 0, 0,
                               Config.wdWidth, Config.wdHeight, nullptr, NULL,
                               hInstance, Config.lParam);

  // SetLayeredWindowAttributes(hwnd_, RGB(0,0,0), 0, ULW_COLORKEY);

  SetProcessDPIAware();

  if (hwnd_ == NULL) {
    OutputDebugString(L"Window Creation Failed\n");
    OutputDebugString((std::to_wstring(GetLastError()) + L"\n").c_str());
    return hwnd_;
  }

  return hwnd_;
}

HWND WinForge::CreateWindowAsync(const wchar_t *window_name,
                                 HINSTANCE &hInstance, int nCmdShow,
                                 D3DDevice D3DDevStruct) {

  std::wstring name(window_name);
  std::thread test1([this, name, hInstance, nCmdShow, D3DDevStruct] {
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

    // ###############################################################################//

    custommainBufferDesc = {};
    custommainBufferDesc.Width = config.wdWidth;
    custommainBufferDesc.Height = config.wdHeight;
    custommainBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    custommainBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    custommainBufferDesc.BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    custommainBufferDesc.SampleDesc.Count = 1;
    custommainBufferDesc.SampleDesc.Quality = 0;
    custommainBufferDesc.ArraySize = 1;
    custommainBufferDesc.MipLevels = 1;
    custommainBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    D3D11Device->CreateTexture2D(&custommainBufferDesc, nullptr,
                                 NvdecBuffer.GetAddressOf());

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    NVDec = new NVDecoder(config.wdWidth, config.wdHeight, NvdecBuffer.Get());

    MainLoop();
  });

  test1.detach();

  return hwnd;
}

void WinForge::Render() {
  hr = D3D11Device->CreateShaderResourceView(NvdecBuffer.Get(), &srvDesc,
                                             textureView.GetAddressOf());
  if (FAILED(hr)) {
    _com_error err(hr);
    OutputDebugString(err.ErrorMessage());
    OutputDebugString(L"aaaaaaaaaaaaaaaa\n");
  }

  D3D11Context->PSSetShaderResources(0, 1, textureView.GetAddressOf());

  D3D11Context->ClearRenderTargetView(renderTargetView, clearColor);
  D3D11Context->OMSetRenderTargets(1, &renderTargetView, nullptr);
  D3D11Context->Draw(4, 0);

  swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

  // D3D11Context->FinishCommandList();
}

void WinForge::MainLoop() {
  while (true) {
    EventDW = MsgWaitForMultipleObjectsEx(1, Events, 0, QS_ALLINPUT, 0);

    switch (EventDW) {
    case WAIT_OBJECT_0 + 1:
      while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

        if (msg.message == WM_QUIT)
          break;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }

      break;

    case WAIT_OBJECT_0 + 0:
      if (std::chrono::steady_clock::now() - LastFrameTime >= FrameTimeLimit) {
        DecodeBuffer();
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

LRESULT CALLBACK WinForge::WProc2(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam) {
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
