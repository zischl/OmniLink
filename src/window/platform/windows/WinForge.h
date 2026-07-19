#ifndef UNICODE
#define UNICODE
#endif

#ifndef WINFORGE_H
#define WINFORGE_H

#pragma once

#include "D3D11Renderer.h"
#include "DecoderConcept.h"
#include "OmniConfig.h"
#include "WinCap.h"
#include "nvdec.h"

#include <Windows.h>
#include <comdef.h>
#include <wrl/client.h>

#include <string>
#include <variant>

using Microsoft::WRL::ComPtr;

struct WinConfig
{
    std::wstring class_name = L"Something";
    const std::wstring Window_Name;
    UINT wdWidth = 1280;
    UINT wdHeight = 720;
    LPVOID lParam = NULL;

    WinConfig(
        const std::wstring ClassName,
        const UINT Width,
        const UINT Height,
        const wchar_t* WindowName,
        LPVOID lParam_
    )
        : class_name(ClassName), Window_Name(WindowName), wdWidth(Width), wdHeight(Height),
          lParam(lParam_)
    {
    }
};

HWND WindowInit(WinConfig& Config, HINSTANCE hInstance, int nCmdShow, WNDPROC WProc);

class WinForge
{
  public:
    WinForge(WNDPROC WindowProc = WProc2);

    HWND CreateWindowAsync(
        const wchar_t* window_name, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct = {}
    );

    inline void SetFrameBufferSize(int Size)
    {
        char* block = new CHAR[FrameQueueSize * FrameSize];
        for (int i = 0; i < FrameQueueSize; i++)
            FramePool[i].FrameBuffer = block + i * FrameSize;
    }

    inline void SetFramePoolSize(int Size)
    {
        if (FramePool != nullptr) {
            delete[] FramePool;
        }

        FramePool = new Frame[Size];
        SetFrameBufferSize(FrameSize);
    }

    inline void SetBufferData(char* Data, int Size)
    {
        memcpy(FramePool[NextFrame].FrameBuffer, Data, Size);

        FramePool[NextFrame].FrameSize = Size;
        NextFrame = NextFrame + 1 & 3;
    }

    inline char* GetFrameDataPool() const { return FramePool[0].FrameBuffer; }

    inline uint32_t GetFrameDataTotalSize() const
    {
        return static_cast<uint32_t>(FrameQueueSize * FrameSize);
    }
    inline uint32_t GetFrameQueueSize() const { return static_cast<uint32_t>(FrameQueueSize); }

    inline void OnFrameUpdate(uint32_t Slot, uint32_t Size)
    {
        FramePool[Slot].FrameSize = static_cast<UINT>(Size);
        NextFrame = static_cast<uint8_t>((Slot + 1) % FrameQueueSize);
        SetRenderEvent();
    }

    inline void DecodeBuffer()
    {
        std::visit(
            [this](auto& ActiveDecoder) {
                using T = std::decay_t<decltype(ActiveDecoder)>;
                if constexpr (!std::is_same_v<T, std::monostate>) {
                    static_assert(
                        DecoderConcept<T>, "Decoder type must satisfy the Decoder concept"
                    );
                    ActiveDecoder.Decode(
                        reinterpret_cast<const unsigned char*>(FramePool[CurrentFrame].FrameBuffer),
                        FramePool[CurrentFrame].FrameSize
                    );
                }
            },
            OmniDecoder
        );
        CurrentFrame = CurrentFrame + 1 & 3;
    }

    inline void SetRenderEvent() { SetEvent(Events[0]); }

    inline void SetFPSLimit(int FPS) { FPSLimitMS.store(1000 / FPS); }

  private:
    HRESULT hr = NULL;
    HWND hwnd = NULL;
    WNDPROC WProc = NULL;
    HANDLE* Events = nullptr;
    DWORD EventDW = NULL;

    std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(15 * 1000000);

    std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
        std::chrono::steady_clock::now();

    ID3D11Device* D3D11Device = nullptr;
    ID3D11DeviceContext* D3D11Context = nullptr;

    IDXGISwapChain3* Swapchain = nullptr;
    ID3D11RenderTargetView* RenderTargetView = nullptr;

    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    ID3D11SamplerState* Sampler = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
    ComPtr<ID3D11ShaderResourceView> TextureView = nullptr;

    UINT Stride = 0;
    UINT Offset = 0;

    float ClearColor[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    D3D11_TEXTURE2D_DESC CustommainBufferDesc = {};
    ComPtr<ID3D11Texture2D> NvdecBuffer;
    using DecoderVariant = std::variant<std::monostate, NvdecSession>;
    DecoderVariant OmniDecoder;

    struct Frame
    {
        CHAR* FrameBuffer;
        UINT FrameSize = 0;
    };

    int FrameSize = 2048 * OmniMTU;
    int FrameQueueSize = 4;
    Frame* FramePool = nullptr;
    uint8_t CurrentFrame = 0;
    uint8_t NextFrame = 0;

    D3D11_DEVICE_CONTEXT_TYPE ContextMode = D3D11_DEVICE_CONTEXT_IMMEDIATE;

    std::atomic<int> FPSLimitMS = 7;
    MSG Msg = {};

    void Render();
    void MainLoop();

    __forceinline void null() {}

    // Streamer Links Window Proc
    static LRESULT CALLBACK WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
