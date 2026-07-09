#ifndef UNICODE
#define UNICODE
#include <array>
#endif

#pragma once
#include "WinCap.h"
#include "WinForge.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

LRESULT CALLBACK LinkerWProcTest(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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

void WinCapTest()
{
    std::cout << "[RUN] WinCapTest\n";

    // Simulating linker window creation
    HINSTANCE HInst = GetModuleHandle(NULL);

    // Renderer init phase 1
    D3D11Renderer Renderer;

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    D3DDevice D3DDevStruct =
        Renderer.CreateD3d11Device(featureLevels, _countof(featureLevels), creationFlags);

    // Window creation
    WinForge WinForge;

    WinConfig WindowConfig(L"Linker Test", 1280, 720, L"Linker Test", NULL);
    HWND hwnd = WindowInit(WindowConfig, HInst, SW_SHOW, LinkerWProcTest);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Renderer init phase 2
    HWNDxD3D11 RendererPtrs;
    RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
    RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
    Renderer.RendererInit(hwnd, WindowConfig.wdWidth, WindowConfig.wdHeight, RendererPtrs);

    ID3D11Device* D3D11Device = RendererPtrs.D3D11Device.Get();
    ID3D11DeviceContext* D3D11Context = RendererPtrs.D3D11Context.Get();

    IDXGISwapChain3* Swapchain = RendererPtrs.swapchain.Get();
    ID3D11RenderTargetView* RenderTargetView = RendererPtrs.renderTargetView.Get();

    HWNDxShaders ShaderPtrs = Renderer.ShadersInit(RendererPtrs.D3D11Device.Get());

    Renderer.SetShaders(RendererPtrs.D3D11Context.Get(), &ShaderPtrs);

    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    ComPtr<ID3D11ShaderResourceView> TextureView = nullptr;

    SrvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = 1;

    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    Viewport.Width = WindowConfig.wdWidth;
    Viewport.Height = WindowConfig.wdHeight;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    D3D11Context->RSSetViewports(1, &Viewport);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // If shit goes down down here then.. go check previous tests, gl ! future me !
    assert(D3D11Device != nullptr);
    assert(D3D11Context != nullptr);
    assert(Swapchain != nullptr);
    assert(RenderTargetView != nullptr);

    float clearColor[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    // DXGI test
    DXGICapture DXGICap;
    try {
        auto DXGIDuplication = DXGICap.InitDXGI(D3D11Device);

        // If this is null it's prolly not gonna work anyway
        ID3D11Texture2D* CapturedBuffer = DXGICap.GetBuffer();

        if (TextureView == nullptr) {
            D3D11Device->CreateShaderResourceView(
                CapturedBuffer, &SrvDesc, TextureView.GetAddressOf()
            );
        }

        if (DXGIDuplication != nullptr && CapturedBuffer != nullptr) {
            std::cout << "[INFO] DXGI Capture initialized. Displaying live output for 10 "
                         "seconds...\n";

            auto Start = std::chrono::steady_clock::now();

            while (std::chrono::steady_clock::now() - Start < std::chrono::seconds(10)) {
                if (DXGICap.AcquireFrame()) {
                    if (CapturedBuffer == nullptr) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(15));
                        continue;
                    }

                    D3D11Context->PSSetShaderResources(0, 1, TextureView.GetAddressOf());
                    D3D11Context->ClearRenderTargetView(RenderTargetView, clearColor);
                    D3D11Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);
                    D3D11Context->Draw(4, 0);

                    Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    } catch (...) {
        std::cout << "[WARN] DXGICapture initialization threw an exception.\n";
    }

    // WGC Base test
    try {
        WGScreenCapture WGCapture{D3D11Device, D3D11Context};

        UINT MonitorWidth = static_cast<UINT>(GetSystemMetrics(SM_CXSCREEN));
        UINT MonitorHeight = static_cast<UINT>(GetSystemMetrics(SM_CYSCREEN));

        ID3D11Texture2D* WGCBuffer = nullptr;
        WGCapture.CreateWGCBuffer(D3D11Device, &WGCBuffer, MonitorWidth, MonitorHeight);

        if (WGCBuffer != nullptr) {
            WGCapture.CreateMonitorCapSession(WGCBuffer, MonitorWidth, MonitorHeight);
            WGCapture.StartSession();

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            WGCapture.AcquireFrame();

            TextureView = nullptr;
            D3D11Device->CreateShaderResourceView(WGCBuffer, &SrvDesc, TextureView.GetAddressOf());
            D3D11Context->PSSetShaderResources(0, 1, TextureView.GetAddressOf());

            std::cout
                << "[INFO] WGC Capture initialized. Displaying live output for 10 seconds...\n";

            auto Start = std::chrono::steady_clock::now();

            MSG Msg = {};
            while (std::chrono::steady_clock::now() - Start < std::chrono::seconds(10)) {
                while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&Msg);
                    DispatchMessage(&Msg);
                }

                if (WGCapture.AcquireFrame()) {
                    D3D11Context->PSSetShaderResources(0, 1, TextureView.GetAddressOf());
                }

                D3D11Context->ClearRenderTargetView(RenderTargetView, clearColor);
                D3D11Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);
                D3D11Context->Draw(4, 0);

                Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }

            WGCapture.CloseSession();
            WGCBuffer->Release();
            WGCBuffer = nullptr;
        }
    } catch (...) {
        std::cout << "[WARN] WGScreenCapture threw an exception.\n";
    }

    // WGC Extended Direct RTV test
    try {
        WGScreenCaptureRTV WGCapture{D3D11Device, D3D11Context};

        UINT MonitorWidth = static_cast<UINT>(GetSystemMetrics(SM_CXSCREEN));
        UINT MonitorHeight = static_cast<UINT>(GetSystemMetrics(SM_CYSCREEN));

        WGCapture.CreateMonitorCapSession(
            MonitorWidth, MonitorHeight, RenderTargetView, Swapchain, clearColor
        );
        WGCapture.StartSession();

        std::cout << "[INFO] WGScreenCaptureRTV initialized. Displaying live output for 10 seconds...\n";

        auto Start = std::chrono::steady_clock::now();
        MSG Msg = {};
        while (std::chrono::steady_clock::now() - Start < std::chrono::seconds(10)) {
            while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&Msg);
                DispatchMessage(&Msg);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        WGCapture.CloseSession();
    } catch (...) {
        std::cout << "[WARN] WGScreenCaptureRTV threw an exception.\n";
    }

    // Clean up
    DestroyWindow(hwnd);
    UnregisterClassW(WindowConfig.class_name.c_str(), HInst);

    std::cout << "[PASS] WinCapTest\n";
}
