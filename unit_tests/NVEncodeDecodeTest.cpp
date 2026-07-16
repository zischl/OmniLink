#pragma once
#include "D3D11Renderer.h"
#include "WinCap.h"
#include "WinForge.h"
#include "nvdec.h"
#include "nvenc.h"
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

static LRESULT CALLBACK NvencTestWProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void NvEncDecTest()
{
    std::cout << "[RUN] NvencTest\n";

    // Renderer Creation
    D3D11Renderer Renderer;

    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    UINT CreationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3DDevice D3DDevStruct;

    try {
        D3DDevStruct =
            Renderer.CreateD3d11Device(FeatureLevels, _countof(FeatureLevels), CreationFlags);
    } catch (const std::exception& e) {
        std::cout << "[ERROR] D3D11 device creation failed with exception: " << e.what() << "\n";
        std::cout << "[FAIL] NvencTest (skipped due to no hardware support)\n";
        return;
    } catch (...) {
        std::cout << "[ERROR] D3D11 device creation failed with unknown exception.\n";
        std::cout << "[FAIL] NvencTest (skipped due to no hardware support)\n";
        return;
    }

    if (D3DDevStruct.D3D11Device == nullptr) {
        std::cout << "[WARN] D3D11 device is null. Skipping hardware tests.\n";
        std::cout << "[PASS] NvencTest (skipped due to no hardware support)\n";
        return;
    }

    // Nvidia encoder and decoder setup
    NVENCODER Nvenc;
    CUresult NvdecState = NVDecoder::Initialize();
    if (NvdecState != CUDA_SUCCESS) {
        std::cout << "[WARN] NVDecoder::Initialize() failed with code: " << NvdecState << "\n";
    }

    UINT MonitorWidth = static_cast<UINT>(GetSystemMetrics(SM_CXSCREEN));
    UINT MonitorHeight = static_cast<UINT>(GetSystemMetrics(SM_CYSCREEN));

    std::cout << "[INFO] Detected Monitor Resolution: " << MonitorWidth << "x" << MonitorHeight
              << "\n";

    // Output texture creation
    ComPtr<ID3D11Texture2D> DXGIOutputTexture;
    ComPtr<ID3D11Texture2D> WGCOutputTexture;
    ComPtr<ID3D11Texture2D> WGCRawTexture;
    ComPtr<ID3D11Texture2D> CustomOutputTexture;

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = MonitorWidth;
    TextureDesc.Height = MonitorHeight;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = D3DDevStruct.D3D11Device->CreateTexture2D(
        &TextureDesc, nullptr, DXGIOutputTexture.GetAddressOf()
    );

    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create output textures. hr = 0x" << std::hex << hr
                  << std::dec << " Skipping tests.\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    hr = D3DDevStruct.D3D11Device->CreateTexture2D(
        &TextureDesc, nullptr, WGCOutputTexture.GetAddressOf()
    );

    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create output textures. hr = 0x" << std::hex << hr
                  << std::dec << " Skipping tests.\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    hr = D3DDevStruct.D3D11Device->CreateTexture2D(
        &TextureDesc, nullptr, WGCRawTexture.GetAddressOf()
    );

    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create output textures. hr = 0x" << std::hex << hr
                  << std::dec << " Skipping tests.\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    hr = D3DDevStruct.D3D11Device->CreateTexture2D(
        &TextureDesc, nullptr, CustomOutputTexture.GetAddressOf()
    );

    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create output textures. hr = 0x" << std::hex << hr
                  << std::dec << " Skipping tests.\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    ComPtr<ID3D11ShaderResourceView> DXGIOutputSRV;
    ComPtr<ID3D11ShaderResourceView> WGCOutputSRV;
    ComPtr<ID3D11ShaderResourceView> WGCRawSRV;
    ComPtr<ID3D11ShaderResourceView> CustomOutputSRV;

    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    SrvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Texture2D.MostDetailedMip = 0;
    SrvDesc.Texture2D.MipLevels = 1;

    hr = D3DDevStruct.D3D11Device->CreateShaderResourceView(
        DXGIOutputTexture.Get(), &SrvDesc, DXGIOutputSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create DXGI SRV. hr = 0x" << std::hex << hr << std::dec
                  << "\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    hr = D3DDevStruct.D3D11Device->CreateShaderResourceView(
        WGCOutputTexture.Get(), &SrvDesc, WGCOutputSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create WGC SRV. hr = 0x" << std::hex << hr << std::dec
                  << "\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    hr = D3DDevStruct.D3D11Device->CreateShaderResourceView(
        WGCRawTexture.Get(), &SrvDesc, WGCRawSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create WGC Raw SRV. hr = 0x" << std::hex << hr << std::dec
                  << "\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    hr = D3DDevStruct.D3D11Device->CreateShaderResourceView(
        CustomOutputTexture.Get(), &SrvDesc, CustomOutputSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        std::cout << "[WARN] Failed to create Custom SRV. hr = 0x" << std::hex << hr << std::dec
                  << "\n";
        std::cout << "[FAIL] NvencTest\n";
        return;
    }

    // Window Setup..
    HINSTANCE HInst = GetModuleHandle(NULL);
    WinForge WinForge;
    WinConfig WindowConfig(L"NvencTestClass", 1280, 720, L"NVENC/NVDEC Integration Test", NULL);
    HWND hwnd = WindowInit(WindowConfig, HInst, SW_SHOW, NvencTestWProc);
    if (hwnd == NULL) {
        std::cout << "[ERROR] WindowInit returned NULL HWND!\n";
        return;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // And.. yes a window needs a renderer setup with swaps and rtv..
    HWNDxD3D11 RendererPtrs;
    RendererPtrs.D3D11Device = D3DDevStruct.D3D11Device;
    RendererPtrs.D3D11Context = D3DDevStruct.D3D11Context;
    Renderer.RendererInit(hwnd, WindowConfig.wdWidth, WindowConfig.wdHeight, RendererPtrs);

    ID3D11Device* D3D11Device = RendererPtrs.D3D11Device.Get();
    ID3D11DeviceContext* D3D11Context = RendererPtrs.D3D11Context.Get();
    IDXGISwapChain3* Swapchain = RendererPtrs.swapchain.Get();
    ID3D11RenderTargetView* RenderTargetView = RendererPtrs.renderTargetView.Get();

    if (D3D11Device == nullptr || D3D11Context == nullptr || Swapchain == nullptr ||
        RenderTargetView == nullptr) {
        std::cout << "[ERROR] RendererInit failed to populate D3D resources!\n";
        DestroyWindow(hwnd);
        UnregisterClassW(WindowConfig.class_name.c_str(), HInst);
        return;
    }

    // and... Shaders..
    HWNDxShaders ShaderPtrs = Renderer.ShadersInit(D3D11Device);
    Renderer.SetShaders(D3D11Context, &ShaderPtrs);

    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = 0.0f;
    Viewport.TopLeftY = 0.0f;
    Viewport.Width = static_cast<float>(WindowConfig.wdWidth);
    Viewport.Height = static_cast<float>(WindowConfig.wdHeight);
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;
    D3D11Context->RSSetViewports(1, &Viewport);

    float ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    // Lambda helper to render and present the frame using the shader view
    auto RenderAndPresent = [&](ID3D11ShaderResourceView* TextureView,
                                const std::wstring& Title,
                                double DurationSeconds) {
        if (TextureView != nullptr) {
            std::wcout << L"[INFO] Rendering: " << Title << L"\n";
            auto Start = std::chrono::steady_clock::now();
            float renderClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

            MSG Msg = {};
            while (std::chrono::steady_clock::now() - Start <
                   std::chrono::duration<double>(DurationSeconds)) {
                while (PeekMessage(&Msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&Msg);
                    DispatchMessage(&Msg);
                }

                D3D11Context->PSSetShaderResources(0, 1, &TextureView);
                D3D11Context->ClearRenderTargetView(RenderTargetView, renderClearColor);
                D3D11Context->OMSetRenderTargets(1, &RenderTargetView, nullptr);
                D3D11Context->Draw(4, 0);

                Swapchain->Present(0, DXGI_PRESENT_ALLOW_TEARING);

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        } else {
            std::cout << "[WARN] Shader Resource View of texture is null.\n";
        }
    };

    // DXGI...
    try {
        std::cout << "[INFO] Initializing DXGI Capture...\n";
        DXGICapture DXGICap;
        auto DXGIDuplication = DXGICap.InitDXGI(D3DDevStruct.D3D11Device.Get());
        if (DXGIDuplication == nullptr) {
            std::cout << "[WARN] DXGI Capture duplication was null. Skipping DXGI test.\n";
        } else {
            ID3D11Texture2D* CaptureTex = DXGICap.GetBuffer();
            if (CaptureTex == nullptr) {
                std::cout << "[WARN] DXGI Capture GetBuffer returned null. Skipping DXGI test.\n";
            } else {
                StaticNvencSession NvencSession(
                    D3DDevStruct.D3D11Device.Get(),
                    Nvenc.NVFunctions,
                    CaptureTex,
                    MonitorWidth,
                    MonitorHeight
                );

                bool FrameCapState = false;
                for (int i = 0; i < 15; ++i) {
                    FrameCapState = DXGICap.AcquireFrame();

                    std::this_thread::sleep_for(std::chrono::milliseconds(33));
                }

                if (!FrameCapState) {
                    std::cout << "[WARN] DXGI: Failed to acquire any frame.\n";
                } else {
                    ComPtr<ID3D11ShaderResourceView> RawCaptureSRV;
                    hr = D3DDevStruct.D3D11Device->CreateShaderResourceView(
                        CaptureTex, &SrvDesc, RawCaptureSRV.GetAddressOf()
                    );
                    if (SUCCEEDED(hr) && RawCaptureSRV != nullptr) {
                        RenderAndPresent(RawCaptureSRV.Get(), L"DXGI Capture - Raw Frame", 3.0);
                    } else {
                        std::cout << "[WARN] DXGI: Failed to create SRV for raw captured frame.\n";
                    }

                    NvencSession.Encode();

                    NV_ENC_LOCK_BITSTREAM& NvLock = NvencSession.NVBitstreamLock;
                    if (NvLock.bitstreamBufferPtr != nullptr && NvLock.bitstreamSizeInBytes > 0) {
                        std::cout << "[INFO] DXGI: Encoded size = " << NvLock.bitstreamSizeInBytes
                                  << " bytes\n";

                        bool DecodeState = false;
                        NvdecSession Nvdec(MonitorWidth, MonitorHeight, DXGIOutputTexture.Get());
                        CUresult NvdecStatus = Nvdec.InitializeSession();
                        if (NvdecStatus == CUDA_SUCCESS) {
                            Nvdec.Decode(
                                static_cast<const unsigned char*>(NvLock.bitstreamBufferPtr),
                                NvLock.bitstreamSizeInBytes
                            );
                            Nvdec.CloseSession();
                            DecodeState = true;
                        } else {
                            std::cout << "[WARN] DXGI: NVDEC InitializeSession failed with code "
                                      << NvdecStatus << "\n";
                        }

                        NvencSession.NVUnlockBitStream();

                        if (DecodeState) {
                            RenderAndPresent(
                                DXGIOutputSRV.Get(), L"DXGI Capture - Decoded Frame", 3.0
                            );
                        } else {
                            std::cout << "[WARN] DXGI: No frame was successfully decoded.\n";
                        }
                    } else {
                        std::cout << "[WARN] DXGI: Encoded bitstream pointer or size is invalid.\n";
                        NvencSession.NVUnlockBitStream();
                    }
                }
            }
        }
    } catch (...) {
        std::cout << "[ERROR] DXGI test block threw structured/unknown exception.\n";
    }

    // WGC capture
    try {
        std::cout << "[INFO] Initializing WGC Capture...\n";
        WGScreenCaptureEx WGCap(D3DDevStruct.D3D11Device.Get());

        std::mutex Mutex;
        std::condition_variable FrameCapCV;
        bool FrameCapState = false;

        WGCap.CreateMonitorCapSession(
            MonitorWidth,
            MonitorHeight,
            [&WGCRawTexture, &D3DDevStruct, &Mutex, &FrameCapCV, &FrameCapState](
                ID3D11Texture2D* Tex2D
            ) {
                if (Tex2D == nullptr) {
                    std::cout << "[WARN] WGC Callback: Received null texture.\n";
                    return;
                }
                {
                    std::lock_guard<std::mutex> lock(Mutex);
                    if (!FrameCapState) {
                        D3DDevStruct.D3D11Context->CopyResource(WGCRawTexture.Get(), Tex2D);
                        FrameCapState = true;
                        FrameCapCV.notify_one();
                    }
                }
                Tex2D->Release();
            }
        );

        WGCap.StartSession();

        {
            std::unique_lock<std::mutex> lock(Mutex);
            FrameCapCV.wait_for(lock, std::chrono::seconds(2), [&FrameCapState] {
                return FrameCapState;
            });
        }

        WGCap.CloseSession();

        if (FrameCapState) {
            RenderAndPresent(WGCRawSRV.Get(), L"WGC Capture - Raw Frame", 3.0);

            StaticNvencSession NvencSession(
                D3DDevStruct.D3D11Device.Get(),
                Nvenc.NVFunctions,
                WGCRawTexture.Get(),
                MonitorWidth,
                MonitorHeight
            );

            NvencSession.Encode();

            NV_ENC_LOCK_BITSTREAM& NvLock = NvencSession.NVBitstreamLock;
            if (NvLock.bitstreamBufferPtr != nullptr && NvLock.bitstreamSizeInBytes > 0) {
                std::cout << "[INFO] WGC: Encoded size = " << NvLock.bitstreamSizeInBytes
                          << " bytes\n";

                // Why is it decode and not Dexode ?
                bool DecodeState = false;
                NvdecSession Nvdec(MonitorWidth, MonitorHeight, WGCOutputTexture.Get());
                CUresult NvdecStatus = Nvdec.InitializeSession();
                if (NvdecStatus == CUDA_SUCCESS) {
                    Nvdec.Decode(
                        static_cast<const unsigned char*>(NvLock.bitstreamBufferPtr),
                        NvLock.bitstreamSizeInBytes
                    );
                    Nvdec.CloseSession();
                    DecodeState = true;
                } else {
                    std::cout << "[WARN] WGC: NVDEC InitializeSession failed with code "
                              << NvdecStatus << "\n";
                }

                NvencSession.NVUnlockBitStream();

                // Display shit
                if (DecodeState) {
                    RenderAndPresent(WGCOutputSRV.Get(), L"WGC Capture - Decoded Frame", 3.0);
                } else {
                    std::cout << "[WARN] WGC: No frame was successfully decoded.\n";
                }
            } else {
                std::cout << "[WARN] WGC: Encoded bitstream pointer or size is invalid.\n";
                NvencSession.NVUnlockBitStream();
            }
        } else {
            std::cout << "[WARN] WGC: No frame was captured.\n";
        }

    } catch (const std::exception& e) {
        std::cout << "[ERROR] WGC test block threw exception: " << e.what() << "\n";
    } catch (...) {
        std::cout << "[ERROR] WGC test block threw structured/unknown exception.\n";
    }

    // Gradient Test Pattern
    try {
        std::cout << "[INFO] Initializing Gradient Test Pattern...\n";
        ComPtr<ID3D11Texture2D> InputTexture;

        D3D11_TEXTURE2D_DESC TempDesc = TextureDesc;
        TempDesc.Usage = D3D11_USAGE_DEFAULT;
        TempDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        std::vector<uint32_t> Pixels(MonitorWidth * MonitorHeight);
        for (int y = 0; y < MonitorHeight; ++y) {
            for (int x = 0; x < MonitorWidth; ++x) {
                uint8_t r = static_cast<uint8_t>(x & 0xFF);
                uint8_t g = static_cast<uint8_t>(y & 0xFF);
                uint8_t b = static_cast<uint8_t>((x + y) & 0xFF);
                uint8_t a = 255;
                Pixels[y * MonitorWidth + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }

        D3D11_SUBRESOURCE_DATA InitData = {};
        InitData.pSysMem = Pixels.data();
        InitData.SysMemPitch = MonitorWidth * 4;

        HRESULT createHr = D3DDevStruct.D3D11Device->CreateTexture2D(
            &TempDesc, &InitData, InputTexture.GetAddressOf()
        );
        if (FAILED(createHr) || InputTexture == nullptr) {
            std::cout << "[ERROR] Failed to create gradient test input texture. hr = 0x" << std::hex
                      << createHr << std::dec << "\n";
            return;
        }

        ComPtr<ID3D11ShaderResourceView> RawGradientSRV;
        hr = D3DDevStruct.D3D11Device->CreateShaderResourceView(
            InputTexture.Get(), &SrvDesc, RawGradientSRV.GetAddressOf()
        );
        if (SUCCEEDED(hr) && RawGradientSRV != nullptr) {
            RenderAndPresent(RawGradientSRV.Get(), L"Gradient Pattern - Raw Frame", 3.0);
        } else {
            std::cout << "[WARN] Gradient: Failed to create SRV for raw gradient frame.\n";
        }

        StaticNvencSession NvencSession(
            D3DDevStruct.D3D11Device.Get(),
            Nvenc.NVFunctions,
            InputTexture.Get(),
            MonitorWidth,
            MonitorHeight
        );

        NvencSession.Encode();

        NV_ENC_LOCK_BITSTREAM& NvLock = NvencSession.NVBitstreamLock;
        if (NvLock.bitstreamBufferPtr == nullptr || NvLock.bitstreamSizeInBytes == 0) {
            std::cout << "[WARN] Gradient: Encoded bitstream is empty.\n";
            NvencSession.NVUnlockBitStream();
            return;
        }

        std::cout << "[INFO] Gradient: Encoded size = " << NvLock.bitstreamSizeInBytes
                  << " bytes\n";

        bool decoded = false;
        NvdecSession Nvdec(MonitorWidth, MonitorHeight, CustomOutputTexture.Get());
        CUresult NvdecStatus = Nvdec.InitializeSession();
        if (NvdecStatus == CUDA_SUCCESS) {
            Nvdec.Decode(
                static_cast<const unsigned char*>(NvLock.bitstreamBufferPtr),
                NvLock.bitstreamSizeInBytes
            );
            Nvdec.CloseSession();
            decoded = true;
        } else {
            std::cout << "[WARN] Gradient: NVDEC InitializeSession failed with code " << NvdecStatus
                      << "\n";
        }
        NvencSession.NVUnlockBitStream();

        if (decoded) {
            RenderAndPresent(CustomOutputSRV.Get(), L"Gradient Pattern - Decoded Frame", 3.0);
        } else {
            std::cout << "[WARN] Gradient: No frame was successfully decoded.\n";
        }

    } catch (const std::exception& e) {
        std::cout << "[ERROR] Gradient test block threw exception: " << e.what() << "\n";
    } catch (...) {
        std::cout << "[ERROR] Gradient test block threw structured/unknown exception.\n";
    }

    // Clean up
    DestroyWindow(hwnd);
    UnregisterClassW(WindowConfig.class_name.c_str(), HInst);

    NVDecoder::Release();
    std::cout << "[PASS] NvencTest\n";
}
