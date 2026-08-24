#ifndef WINCAP_H
#define WINCAP_H

#pragma once
#include "CaptureTypes.h"

#include <functional>
#include <mutex>

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_5.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/windows.graphics.capture.h>
#include <wrl/client.h>

#pragma comment(lib, "windowsapp.lib")

using Microsoft::WRL::ComPtr;

// DXGI Screen Capture class, technically... it's not necessary to ReleaseFrame() anymore with this
// design since it's ComPtr plus each frame aquisition already releases the frame.
class DXGICapture
{
  private:
    HRESULT hr;

    ComPtr<IDXGIOutputDuplication> DXGIOutDuplication;

    DXGI_OUTDUPL_FRAME_INFO FrameInfo;
    ComPtr<IDXGIResource> FramePixelData = nullptr;

    // Returned by GetBuffer, which is where the latest frames are stored
    ComPtr<ID3D11Texture2D> DXGIComBuffer = nullptr;
    bool CaptureState = false;

  public:
    // Run this once and use GetBuffer, already aquires a frame once to get the Tex2D interface
    ComPtr<IDXGIOutputDuplication> InitDXGI(ID3D11Device* D3D11Device);

    ID3D11Texture2D* GetBuffer() const;

    // Blocks up to 33ms which is around min 30 fps waiting for a desktop update
    // returns false on timeout.
    bool AcquireFrame();

    // Not really necessary to call but just in case it's needed to release resources.
    void ReleaseFrame();

    static constexpr FrameAquisition FrameAqMode = FrameAquisition::Polling;
    static constexpr CaptureAPI Type = CaptureAPI::DXGI;
};

// This class will be the base class for WGC Screen Capture and later on WGC Window Capture
class WGCapture
{
  public:
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice D3DDevice_WGC{nullptr};

    ID3D11DeviceContext* D3D11Context = nullptr;

    // Fighting between two shitty wrappers
    inline void SetWrappedD3D11Device(ID3D11Device* D3D11DevicePtr)
    {
        ComPtr<ID3D11Device> ComID3D11Device = D3D11DevicePtr;
        ComPtr<IDXGIDevice> DXGIDevice;
        ComID3D11Device.As(&DXGIDevice);

        winrt::com_ptr<IInspectable> inspectableSurface;
        if (SUCCEEDED(
                CreateDirect3D11DeviceFromDXGIDevice(DXGIDevice.Get(), inspectableSurface.put())
            )) {
            D3DDevice_WGC =
                inspectableSurface
                    .as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        }
    }

    inline HMONITOR GetActiveMonitor()
    {
        HWND WindowHandle = GetForegroundWindow();
        HMONITOR ActiveMonitor = MonitorFromWindow(WindowHandle, MONITOR_DEFAULTTONEAREST);
        return ActiveMonitor;
    }

    // So what if i'm still on classics..
    inline void GetRoActivationFactory(void** WGCInterop)
    {
        HSTRING ClassName;

        winrt::check_hresult(WindowsCreateString(
            L"Windows.Graphics.Capture.GraphicsCaptureItem",
            wcslen(L"Windows.Graphics.Capture.GraphicsCaptureItem"),
            &ClassName
        ));

        winrt::check_hresult(
            RoGetActivationFactory(ClassName, __uuidof(IGraphicsCaptureItemInterop), WGCInterop)
        );
    }

    void GetActiveMonitorCaptureItem(
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem& CaptureItem
    );

    void GetWindowCaptureItem(
        HWND WindowHandle, winrt::Windows::Graphics::Capture::GraphicsCaptureItem& CaptureItem
    );

    void SetCaptureBorderState(
        winrt::Windows::Graphics::Capture::GraphicsCaptureSession& Session, bool State
    );

    // Only needed in copy based WGScreenCapture, otherwise the resource's D3DTexture2D interface
    // can be directly accessed
    void
    CreateWGCBuffer(ID3D11Device* D3D11Device, ID3D11Texture2D** Buffer, UINT Width, UINT Height);
};

// This class will be handling screen capture based on WGC Copy Based, not optimized for networking,
// best for testing purposes, Create a session, Start and and Setup Buffer, Use AcquireFrame
class WGScreenCapture : public WGCapture
{
  private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession Session{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool FramePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem{nullptr};

    std::mutex FrameMutex;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame LatestFrame{nullptr};
    bool FrameAvailability = false;

    ID3D11Texture2D* WBuffer = nullptr;

  public:
    WGScreenCapture(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11Context_);
    ~WGScreenCapture();

    void CreateMonitorCapSession(ID3D11Texture2D* Buffer, UINT Width, UINT Height);

    // Copies the latest WGC frame into WBuffer on the calling thread.
    // Returns true if a new frame was available and successfully copied.
    bool AcquireFrame();

    void StartSession();
    void CloseSession();
};

// Optimised WGC screen capture with no mutex, no copies, no states, no extra threads
// Callback accepted in session creation which receives ID3D11Texture2D*
// Do release that texture after use which frees up WGC Pool Slot for a new frame
class WGScreenCaptureEx : public WGCapture
{
  public:
    static constexpr FrameAquisition FrameAqMode = FrameAquisition::EventDriven;
    static constexpr CaptureAPI Type = CaptureAPI::WGC;

    using FrameCallback = std::function<void(ID3D11Texture2D*)>;

    explicit WGScreenCaptureEx(ID3D11Device* D3D11DevicePtr);

    // Registers the callback, builds the 3 slot frame pool, and
    // creates the capture session. Call StartSession() to.. uh... start.
    // And.. CloseSession() to.. well.. close and cleanup
    // 3 slots designed each for processing state, ready state and latest frame capture
    // DO EFFING REMEMBER TO RELEASE THE TEXTURE2D WHEN DONE USING
    void CreateMonitorCapSession(UINT Width, UINT Height, FrameCallback OnFrameCallback);

    void StartSession();
    void CloseSession();

  private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession Session{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool FramePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem{nullptr};

    FrameCallback OnFrameArrived;
};

// Same as WGScreenCaptureEx on optimizations but mainly for displaying to a Render Target View
// while utilizing an SRV Cache.
class WGScreenCaptureRTV : public WGCapture
{
  public:
    static constexpr FrameAquisition FrameAqMode = FrameAquisition::EventDriven;
    static constexpr CaptureAPI Type = CaptureAPI::WGC;

    WGScreenCaptureRTV(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11ContextPtr);
    ~WGScreenCaptureRTV();

    void CreateMonitorCapSession(
        UINT Width,
        UINT Height,
        ID3D11RenderTargetView* RenderTargetView,
        IDXGISwapChain* Swapchain,
        const float ClearColor[4]
    );

    void StartSession();
    void CloseSession();

  private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession Session{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool FramePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem{nullptr};

    ID3D11Device* D3D11Device = nullptr;
    ID3D11DeviceContext* D3D11Context = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    float ClearCol[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    struct SRVCacheSlot
    {
        IUnknown* SurfacePtr = nullptr;
        ComPtr<ID3D11ShaderResourceView> TextureView = nullptr;
    };
    std::array<SRVCacheSlot, 3> SRVCache;
};

// This class will be handling window capture based on WGC Copy Based, not optimized for networking,
// best for testing purposes, Create a session, Start and and Setup Buffer, Use AcquireFrame
class WGWindowCapture : public WGCapture
{
  private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession Session{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool FramePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem{nullptr};
    winrt::event_token ClosedToken;

    std::mutex FrameMutex;
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFrame LatestFrame{nullptr};
    bool FrameAvailability = false;

    ID3D11Texture2D* WBuffer = nullptr;
    HWND TargetHwnd = NULL;
    int CurrentWidth = 0;
    int CurrentHeight = 0;

  public:
    WGWindowCapture(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11Context_);
    ~WGWindowCapture();

    void CreateWindowCapSession(
        HWND WindowHandle, ID3D11Texture2D* Buffer, UINT Width = 0, UINT Height = 0
    );

    inline void CreateMonitorCapSession(ID3D11Texture2D* Buffer, UINT Width = 0, UINT Height = 0)
    {
        CreateWindowCapSession(GetForegroundWindow(), Buffer, Width, Height);
    }

    // Copies the latest WGC frame into WBuffer on the calling thread.
    // Returns true if a new frame was available and successfully copied.
    bool AcquireFrame();

    void StartSession();
    void CloseSession();
};

// Optimised WGC window capture with no mutex, no copies, no states, no extra threads
// Callback accepted in session creation which receives ID3D11Texture2D*
// Do release that texture after use which frees up WGC Pool Slot for a new frame
class WGWindowCaptureEx : public WGCapture
{
  public:
    static constexpr FrameAquisition FrameAqMode = FrameAquisition::EventDriven;
    static constexpr CaptureAPI Type = CaptureAPI::WGC;

    using FrameCallback = std::function<void(ID3D11Texture2D*)>;

    explicit WGWindowCaptureEx(ID3D11Device* D3D11DevicePtr);
    ~WGWindowCaptureEx();

    // Registers the callback, builds the 3 slot frame pool, and
    // creates the capture session. Call StartSession() to.. uh... start.
    // And.. CloseSession() to.. well.. close and cleanup
    // 3 slots designed each for processing state, ready state and latest frame capture
    // DO EFFING REMEMBER TO RELEASE THE TEXTURE2D WHEN DONE USING
    void CreateWindowCapSession(
        HWND WindowHandle, UINT Width, UINT Height, FrameCallback OnFrameCallback
    );
    void CreateWindowCapSession(HWND WindowHandle, FrameCallback OnFrameCallback);

    inline void CreateMonitorCapSession(UINT Width, UINT Height, FrameCallback OnFrameCallback)
    {
        CreateWindowCapSession(GetForegroundWindow(), Width, Height, std::move(OnFrameCallback));
    }

    void StartSession();
    void CloseSession();

  private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession Session{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool FramePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem{nullptr};
    winrt::event_token ClosedToken;

    FrameCallback OnFrameArrived;
    HWND TargetHWnd = NULL;
    int CurrentWidth = 0;
    int CurrentHeight = 0;
};

// Same as WGWindowCaptureEx on optimizations but mainly for displaying to a Render Target View
// while utilizing an SRV Cache.
class WGWindowCaptureRTV : public WGCapture
{
  public:
    static constexpr FrameAquisition FrameAqMode = FrameAquisition::EventDriven;
    static constexpr CaptureAPI Type = CaptureAPI::WGC;

    WGWindowCaptureRTV(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11ContextPtr);
    ~WGWindowCaptureRTV();

    void CreateWindowCapSession(
        HWND WindowHandle,
        UINT Width,
        UINT Height,
        ID3D11RenderTargetView* RenderTargetView,
        IDXGISwapChain* Swapchain,
        const float ClearColor[4] = nullptr
    );

    inline void CreateMonitorCapSession(
        UINT Width,
        UINT Height,
        ID3D11RenderTargetView* RenderTargetView,
        IDXGISwapChain* Swapchain,
        const float ClearColor[4] = nullptr
    )
    {
        CreateWindowCapSession(
            GetForegroundWindow(), Width, Height, RenderTargetView, Swapchain, ClearColor
        );
    }

    void StartSession();
    void CloseSession();

  private:
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession Session{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool FramePool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem CaptureItem{nullptr};
    winrt::event_token ClosedToken;

    ID3D11Device* D3D11Device = nullptr;
    ID3D11DeviceContext* D3D11Context = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
    IDXGISwapChain* SwapChain = nullptr;
    float ClearCol[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    HWND TargetHWnd = NULL;
    int CurrentWidth = 0;
    int CurrentHeight = 0;

    struct SRVCacheSlot
    {
        IUnknown* SurfacePtr = nullptr;
        ComPtr<ID3D11ShaderResourceView> TextureView = nullptr;
    };
    std::array<SRVCacheSlot, 3> SRVCache;

    void ClearSRVCache();
};

#endif
