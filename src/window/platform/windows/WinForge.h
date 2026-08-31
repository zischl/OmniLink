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

#include <atomic>
#include <string>
#include <thread>
#include <variant>

using Microsoft::WRL::ComPtr;

struct WinConfig
{
    std::wstring       class_name = L"Something";
    const std::wstring Window_Name;
    UINT               wdWidth  = 1280;
    UINT               wdHeight = 720;
    LPVOID             lParam   = NULL;

    WinConfig(
        const std::wstring ClassName,
        const UINT         Width,
        const UINT         Height,
        const wchar_t*     WindowName,
        LPVOID             lParam_
    )
        : class_name(ClassName), Window_Name(WindowName), wdWidth(Width), wdHeight(Height),
          lParam(lParam_)
    {
    }
};

HWND WindowInit(WinConfig& Config, HINSTANCE hInstance, int nCmdShow, WNDPROC WProc);

constexpr UINT WM_SWAP_DECODER = WM_USER + 101;

class WinForge
{
  public:
    WinForge(WNDPROC WindowProc = WProc2);
    ~WinForge();

    WinForge(const WinForge&)            = delete;
    WinForge& operator=(const WinForge&) = delete;

    HWND CreateWindowAsync(
        const wchar_t* window_name, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct = {}
    );

    inline void SetFrameBufferSize(int Size)
    {
        if (RawFrameBufferBlock != nullptr) {
            delete[] RawFrameBufferBlock;
            RawFrameBufferBlock = nullptr;
        }

        FrameSize           = Size;
        RawFrameBufferBlock = new CHAR[FrameQueueSize * FrameSize];
        for (int i = 0; i < FrameQueueSize; i++)
            FramePool[i].FrameBuffer = RawFrameBufferBlock + i * FrameSize;
    }

    inline void SetFramePoolSize(int Size)
    {
        CleanupFramePool();

        FrameQueueSize = Size;
        FramePool      = new Frame[FrameQueueSize];
        SetFrameBufferSize(FrameSize);
    }

    inline void SetBufferData(char* Data, int Size)
    {
        uint32_t Slot =
            static_cast<uint32_t>(QueuedCount.load(std::memory_order_relaxed) % FrameQueueSize);
        memcpy(FramePool[Slot].FrameBuffer, Data, Size);
        FramePool[Slot].FrameSize = Size;
        QueuedCount.fetch_add(1, std::memory_order_release);
        SetRenderEvent();
    }

    inline char* GetFrameDataPool() const { return FramePool[0].FrameBuffer; }

    inline uint32_t GetFrameDataTotalSize() const
    {
        return static_cast<uint32_t>(FrameQueueSize * FrameSize);
    }

    inline uint32_t GetFrameQueueSize() const { return static_cast<uint32_t>(FrameQueueSize); }

    // Packages up the frame pool for wiring into a sub-stream recv pool.
    // OnSlotComplete fires from the IOCP thread — routes through OnFrameUpdate then sets render
    // event.
    inline void GetFramePool(
        char*&    OutData,
        uint32_t& OutDataSize,
        uint32_t& OutSlotCount,
        void (**OutOnSlotComplete)(void*, uint32_t, uint32_t),
        void*& OutCtx
    )
    {
        OutData            = GetFrameDataPool();
        OutDataSize        = GetFrameDataTotalSize();
        OutSlotCount       = GetFrameQueueSize();
        *OutOnSlotComplete = [](void* ctx, uint32_t slot, uint32_t size) {
            reinterpret_cast<WinForge*>(ctx)->OnFrameUpdate(slot, size);
        };
        OutCtx = this;
    }

    inline void OnFrameUpdate(uint32_t Slot, uint32_t Size)
    {
        FramePool[Slot].FrameSize = static_cast<UINT>(Size);
        QueuedCount.fetch_add(1, std::memory_order_release);
        SetRenderEvent();
    }

    inline void DecodeBuffer(NvdecSession* ActiveDecoder)
    {
        uint64_t Decoded = QueuedCount.load(std::memory_order_acquire);
        while (DecodedCount < Decoded) {
            uint32_t slot = static_cast<uint32_t>(DecodedCount % FrameQueueSize);
            if (ActiveDecoder != nullptr) {
                ActiveDecoder->Decode(
                    reinterpret_cast<const unsigned char*>(FramePool[slot].FrameBuffer),
                    FramePool[slot].FrameSize
                );
            }
            ++DecodedCount;
        }
    }

    inline void DecodeBuffer()
    {
        NvdecSession* activeDecoder = std::get_if<NvdecSession>(&OmniDecoder);
        DecodeBuffer(activeDecoder);
    }

    template <typename T, typename... Args> inline void SwapDecoder(Args&&... args)
    {
        static_assert(DecoderConcept<T>, "Decoder type must satisfy the Decoder concept");
        OmniDecoder.emplace<T>(std::forward<Args>(args)...);
        if (hwnd != NULL) {
            PostMessage(hwnd, WM_SWAP_DECODER, 0, 0);
        }
    }

    inline void SetRenderEvent() { SetEvent(Events[0]); }

    inline void SetFPSLimit(int FPS)
    {
        FrameTimeLimit = std::chrono::nanoseconds(1000000000LL / FPS);
    }

  private:
    HRESULT     hr      = NULL;
    HWND        hwnd    = NULL;
    WNDPROC     WProc   = NULL;
    HANDLE*     Events  = nullptr;
    DWORD       EventDW = NULL;
    std::thread WindowThread;

    std::chrono::steady_clock::duration FrameTimeLimit =
        std::chrono::nanoseconds(1000000000LL / 75);

    std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
        std::chrono::steady_clock::now();

    ID3D11Device*        D3D11Device  = nullptr;
    ID3D11DeviceContext* D3D11Context = nullptr;

    IDXGISwapChain3*        Swapchain        = nullptr;
    ID3D11RenderTargetView* RenderTargetView = nullptr;

    ID3D11PixelShader*               PixelShader  = nullptr;
    ID3D11VertexShader*              VertexShader = nullptr;
    ID3D11Buffer*                    VertexBuffer = nullptr;
    ID3D11InputLayout*               InputLayout  = nullptr;
    ID3D11Buffer*                    IndexBuffer  = nullptr;
    ID3D11SamplerState*              Sampler      = nullptr;
    D3D11_SHADER_RESOURCE_VIEW_DESC  SrvDesc      = {};
    ComPtr<ID3D11ShaderResourceView> TextureView  = nullptr;

    UINT Stride = 0;
    UINT Offset = 0;

    float ClearColor[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    D3D11_TEXTURE2D_DESC    CustommainBufferDesc = {};
    ComPtr<ID3D11Texture2D> FrameBufferTex;
    using DecoderVariant = std::variant<std::monostate, NvdecSession>;
    DecoderVariant OmniDecoder;

    struct Frame
    {
        CHAR* FrameBuffer;
        UINT  FrameSize = 0;
    };

    int                   FrameSize           = 2048 * OmniMTU;
    int                   FrameQueueSize      = 4;
    CHAR*                 RawFrameBufferBlock = nullptr;
    Frame*                FramePool           = nullptr;
    std::atomic<uint64_t> QueuedCount{0};
    uint64_t              DecodedCount = 0;

    D3D11_DEVICE_CONTEXT_TYPE ContextMode = D3D11_DEVICE_CONTEXT_IMMEDIATE;

    MSG Msg = {};

    void        Render();
    void        MainLoop();
    void        CloseWindowThread();
    void        CleanupD3D();
    void        CleanupEvents();
    inline void CleanupFramePool()
    {
        QueuedCount.store(0, std::memory_order_relaxed);
        DecodedCount = 0;
        if (RawFrameBufferBlock != nullptr) {
            delete[] RawFrameBufferBlock;
            RawFrameBufferBlock = nullptr;
        }
        if (FramePool != nullptr) {
            delete[] FramePool;
            FramePool = nullptr;
        }
    }

    __forceinline void null() {}

    // Streamer Links Window Proc
    static LRESULT CALLBACK WProc2(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#endif
