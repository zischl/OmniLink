#pragma once
#include "D3D11Renderer.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

static LRESULT CALLBACK D3DTestWProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void D3D11RendererTest()
{
    std::cout << "[RUN] D3D11RendererTest\n";

    HINSTANCE hInst = GetModuleHandle(NULL);
    WNDCLASSEXW wc = {};
    wc.lpfnWndProc = D3DTestWProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"D3DTestClass";
    wc.cbSize = sizeof(WNDCLASSEXW);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        L"D3DTestClass",
        L"D3D Test Window",
        WS_POPUP | WS_VISIBLE,
        100,
        100,
        800,
        600,
        nullptr,
        NULL,
        hInst,
        nullptr
    );
    assert(hwnd != NULL);

    // Initializing D3D11 renderer
    D3D11Renderer renderer;
    HWNDxD3D11 d3dStruct;

    // Calling upon RendererInit which creates(again.. hopefully) D3D11 Device, SwapChain, and
    // Render Target View
    renderer.RendererInit(hwnd, 800, 600, d3dStruct);

    // Null checks for D3D components
    assert(d3dStruct.D3D11Device != nullptr);
    assert(d3dStruct.D3D11Context != nullptr);
    assert(d3dStruct.swapchain != nullptr);
    assert(d3dStruct.renderTargetView != nullptr);

    // Should be seeing 3 different clear colors: Red, Green, Blue
    float colors[3][4] = {
        {1.0f, 0.0f, 0.0f, 1.0f}, // Red
        {0.0f, 1.0f, 0.0f, 1.0f}, // Green
        {0.0f, 0.0f, 1.0f, 1.0f}  // Blue
    };

    for (int i = 0; i < 3; ++i) {
        d3dStruct.D3D11Context->ClearRenderTargetView(d3dStruct.renderTargetView.Get(), colors[i]);

        d3dStruct.swapchain->Present(1, 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Cleanup
    DestroyWindow(hwnd);
    UnregisterClassW(L"D3DTestClass", hInst);

    std::cout << "[PASS] D3D11RendererTest\n";
}
