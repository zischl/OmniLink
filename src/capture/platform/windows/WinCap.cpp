#include "WinCap.h"
#include "OmniLogger.h"

#define NullCheck(item, text)                                                                      \
    {                                                                                              \
        if (item == nullptr) {                                                                     \
            Logger::log(text);                                                                     \
        }                                                                                          \
    }

// DXGICapture

ComPtr<IDXGIOutputDuplication> DXGICapture::InitDXGI(ID3D11Device* D3D11Device)
{

    ComPtr<IDXGIDevice> DXGIDevice;
    hr = D3D11Device->QueryInterface(IID_PPV_ARGS(&DXGIDevice));
    HRCheck(hr);

    ComPtr<IDXGIAdapter> DXGIAdapter;
    hr = DXGIDevice->GetAdapter(&DXGIAdapter);
    HRCheck(hr);

    ComPtr<IDXGIOutput> DXGIOutput;
    hr = DXGIAdapter->EnumOutputs(0, &DXGIOutput);
    HRCheck(hr);

    ComPtr<IDXGIOutput1> DXGIOutputEnhanced;
    hr = DXGIOutput->QueryInterface(IID_PPV_ARGS(&DXGIOutputEnhanced));
    HRCheck(hr);

    hr = DXGIOutputEnhanced->DuplicateOutput(D3D11Device, &DXGIOutDuplication);
    HRCheck(hr);

    hr = DXGIOutDuplication->AcquireNextFrame(500, &FrameInfo, &FramePixelData);
    if (SUCCEEDED(hr)) {
        FramePixelData.As(&DXGIComBuffer);
        CaptureState = true;
    }

    return DXGIOutDuplication;
}

ID3D11Texture2D* DXGICapture::GetBuffer() const
{
    return DXGIComBuffer.Get();
}

bool DXGICapture::AcquireFrame()
{
    if (CaptureState) {
        DXGIOutDuplication->ReleaseFrame();
        CaptureState = false;
    }

    hr = DXGIOutDuplication->AcquireNextFrame(0, &FrameInfo, &FramePixelData);
    if (SUCCEEDED(hr)) {
        CaptureState = true;
        return true;
    }

    return false;
}

void DXGICapture::ReleaseFrame()
{
    if (CaptureState) {
        DXGIOutDuplication->ReleaseFrame();
        CaptureState = false;
    }
}

// WGCapture

namespace winrt {
using namespace Windows::Graphics;
using namespace Windows::Graphics::Capture;
using namespace Windows::Graphics::DirectX;
using namespace Windows::Graphics::DirectX::Direct3D11;
} // namespace winrt

void WGCapture::GetActiveMonitorCaptureItem(
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem& CaptureItem
)
{

    winrt::com_ptr<IGraphicsCaptureItemInterop> WGCInterop;

    GetRoActivationFactory(WGCInterop.put_void());
    NullCheck(WGCInterop.get(), "WGC Item Interop Get Failed for Monitor Capture\n");

    winrt::check_hresult(WGCInterop->CreateForMonitor(
        GetActiveMonitor(),
        winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
        winrt::put_abi(CaptureItem)
    ));
}

void WGCapture::GetWindowCaptureItem(
    HWND WindowHandle, winrt::Windows::Graphics::Capture::GraphicsCaptureItem& CaptureItem
)
{
    if (WindowHandle == NULL || !IsWindow(WindowHandle)) {
        Logger::log("WGC Item Interop: Invalid HWND provided for Window Capture\n");
        return;
    }

    winrt::com_ptr<IGraphicsCaptureItemInterop> WGCInterop;

    GetRoActivationFactory(WGCInterop.put_void());
    NullCheck(WGCInterop.get(), "WGC Item Interop Get Failed for Window Capture\n");

    winrt::check_hresult(WGCInterop->CreateForWindow(
        WindowHandle,
        winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
        winrt::put_abi(CaptureItem)
    ));
}

void WGCapture::SetCaptureBorderState(
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession& Session, bool Enabled
)
{
    if (!Session)
        return;

    try {
        Session.IsBorderRequired(Enabled);
    } catch (...) {
        // Unsupported on older Windows OS builds
    }
}

void WGCapture::CreateWGCBuffer(
    ID3D11Device* D3D11Device, ID3D11Texture2D** Buffer, UINT Width, UINT Height
)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = Width;
    desc.Height = Height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = D3D11Device->CreateTexture2D(&desc, nullptr, Buffer);
    if (FAILED(hr)) {
        Logger::log((std::to_string(hr) + " WGC Output Buffer Creation Failed\n").c_str());
    }
}

// WGScreenCapture

WGScreenCapture::WGScreenCapture(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11Context_)
{
    D3D11Context = D3D11Context_;

    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    winrt::com_ptr<IGraphicsCaptureItemInterop> WGCInterop;

    GetRoActivationFactory(WGCInterop.put_void());

    SetWrappedD3D11Device(D3D11DevicePtr);
}

WGScreenCapture::~WGScreenCapture()
{
    CloseSession();
}

void WGScreenCapture::CreateMonitorCapSession(ID3D11Texture2D* Buffer, UINT Width, UINT Height)
{
    WBuffer = Buffer;

    GetActiveMonitorCaptureItem(CaptureItem);

    winrt::SizeInt32 Dimensions;
    Dimensions.Width = Width;
    Dimensions.Height = Height;

    FramePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, Dimensions
    );

    FramePool.FrameArrived([this](auto& Pool, auto&) {
        auto frame = Pool.TryGetNextFrame();
        if (frame) {
            std::lock_guard<std::mutex> lock(FrameMutex);
            LatestFrame = std::move(frame);
            FrameAvailability = true;
        }
    });

    NullCheck(D3DDevice_WGC, "D3DDevice Not Set\n");
    NullCheck(FramePool, "WGC FramePool Creation Failed\n");
    NullCheck(CaptureItem, "WGC Capture Item Creation Failed\n");
    NullCheck(WBuffer, "Write Buffer Not Set\n");

    if (CaptureItem != nullptr) {
        Session = FramePool.CreateCaptureSession(CaptureItem);
        Session.IsCursorCaptureEnabled(false);
        NullCheck(Session, "CaptureSession Creation Failed \n");
    }
}

bool WGScreenCapture::AcquireFrame()
{
    std::lock_guard<std::mutex> lock(FrameMutex);
    if (!FrameAvailability || !LatestFrame)
        return false;

    bool copied = false;
    try {
        auto surface = LatestFrame.Surface();
        auto access =
            surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ComPtr<ID3D11Texture2D> surfaceTex;
        if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&surfaceTex)))) {
            D3D11Context->CopyResource(WBuffer, surfaceTex.Get());
            copied = true;
        }
    } catch (...) {
        Logger::log("WGC AcquireFrame: failed to get surface texture\n");
    }

    LatestFrame = nullptr;
    FrameAvailability = false;
    return copied;
}

void WGScreenCapture::StartSession()
{
    NullCheck(Session, "CaptureSession Not Found\n");
    Session.StartCapture();
}

void WGScreenCapture::CloseSession()
{
    {
        std::lock_guard<std::mutex> lock(FrameMutex);
        LatestFrame = nullptr;
        FrameAvailability = false;
    }

    if (Session) {
        Session.Close();
        Session = nullptr;
    }

    if (FramePool) {
        FramePool.Close();
        FramePool = nullptr;
    }
}

// WGScreenCaptureEx

WGScreenCaptureEx::WGScreenCaptureEx(ID3D11Device* D3D11DevicePtr)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    SetWrappedD3D11Device(D3D11DevicePtr);
}

void WGScreenCaptureEx::CreateMonitorCapSession(
    UINT Width, UINT Height, FrameCallback OnFrameCallback
)
{
    OnFrameArrived = std::move(OnFrameCallback);

    GetActiveMonitorCaptureItem(CaptureItem);

    NullCheck(D3DDevice_WGC, "WGStreamCapture: D3DDevice not set\n");
    NullCheck(CaptureItem, "WGStreamCapture: CaptureItem creation failed\n");

    winrt::SizeInt32 Dims;
    Dims.Width = static_cast<int32_t>(Width);
    Dims.Height = static_cast<int32_t>(Height);

    FramePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 3, Dims
    );

    NullCheck(FramePool, "WGStreamCapture: FramePool creation failed\n");

    FramePool.FrameArrived([this](auto& Pool, auto&) {
        auto frame = Pool.TryGetNextFrame();
        if (!frame)
            return;

        auto access =
            frame.Surface()
                .as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ID3D11Texture2D* tex = nullptr;
        // +1 Ref so again.. DO RELEASE IT FROM THE CALLBACK
        if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&tex)))) {
            OnFrameArrived(tex);
        }
    });

    Session = FramePool.CreateCaptureSession(CaptureItem);
    Session.IsCursorCaptureEnabled(false);

    NullCheck(Session, "WGStreamCapture: CaptureSession creation failed\n");
}

void WGScreenCaptureEx::StartSession()
{
    NullCheck(Session, "WGStreamCapture: Session not initialized before StartSession\n");
    Session.StartCapture();
}

void WGScreenCaptureEx::CloseSession()
{
    OnFrameArrived = nullptr;

    if (Session) {
        Session.Close();
        Session = nullptr;
    }
    if (FramePool) {
        FramePool.Close();
        FramePool = nullptr;
    }
}

// WGScreenCaptureRTV

WGScreenCaptureRTV::WGScreenCaptureRTV(
    ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11ContextPtr
)
{
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    SetWrappedD3D11Device(D3D11DevicePtr);
    D3D11Device = D3D11DevicePtr;
    D3D11Context = D3D11ContextPtr;
}

WGScreenCaptureRTV::~WGScreenCaptureRTV()
{
    CloseSession();
}

void WGScreenCaptureRTV::CreateMonitorCapSession(
    UINT Width,
    UINT Height,
    ID3D11RenderTargetView* RenderTargetView,
    IDXGISwapChain* Swapchain,
    const float ClearColor[4]
)
{
    RTV = RenderTargetView;
    SwapChain = Swapchain;
    if (ClearColor != nullptr) {
        memcpy(ClearCol, ClearColor, sizeof(ClearCol));
    }

    GetActiveMonitorCaptureItem(CaptureItem);

    NullCheck(D3DDevice_WGC, "WGScreenCaptureRTV: D3DDevice not set\n");
    NullCheck(CaptureItem, "WGScreenCaptureRTV: CaptureItem creation failed\n");

    winrt::SizeInt32 Dims;
    Dims.Width = static_cast<int32_t>(Width);
    Dims.Height = static_cast<int32_t>(Height);

    FramePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 3, Dims
    );

    NullCheck(FramePool, "WGScreenCaptureRTV: FramePool creation failed\n");

    // Cache pool tests revealed up to 10.7 times faster SRV cache retrieval
    FramePool.FrameArrived([this](auto& Pool, auto&) {
        // auto start = std::chrono::steady_clock::now();

        auto frame = Pool.TryGetNextFrame();
        if (!frame)
            return;

        auto access =
            frame.Surface()
                .as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ID3D11Texture2D* tex = nullptr;
        if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&tex)))) {
            IUnknown* TextureID = nullptr;
            if (SUCCEEDED(
                    tex->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&TextureID))
                )) {
                TextureID->Release();
            }

            ID3D11ShaderResourceView* TextureView = nullptr;

            // Gimme cache hits !
            for (const auto& Slot : SRVCache) {
                if (Slot.SurfacePtr == TextureID && Slot.TextureView != nullptr) {
                    TextureView = Slot.TextureView.Get();

                    /* auto end = std::chrono::steady_clock::now();
                    std::chrono::duration<double> elapsed = end - start;
                    std::cout << "Time elapsed Cache Hit: " << elapsed.count() << " seconds\n"; */

                    break;
                }
            }

            // Cache missed, create SRV and cache it
            if (TextureView == nullptr) {
                D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
                SrvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SrvDesc.Texture2D.MostDetailedMip = 0;
                SrvDesc.Texture2D.MipLevels = 1;

                ComPtr<ID3D11ShaderResourceView> NewTextureView;
                if (SUCCEEDED(D3D11Device->CreateShaderResourceView(
                        tex, &SrvDesc, NewTextureView.GetAddressOf()
                    ))) {
                    TextureView = NewTextureView.Get();

                    for (auto& Slot : SRVCache) {
                        if (Slot.SurfacePtr == nullptr) {
                            Slot.SurfacePtr = TextureID;
                            Slot.TextureView = std::move(NewTextureView);
                            break;
                        }
                    }
                }

                /* auto end = std::chrono::steady_clock::now();
                std::chrono::duration<double> elapsed = end - start;
                std::cout << "Time elapsed Cache Miss: " << elapsed.count() << " seconds\n"; */
            }

            if (TextureView != nullptr && RTV != nullptr && SwapChain != nullptr) {
                D3D11Context->PSSetShaderResources(0, 1, &TextureView);
                D3D11Context->ClearRenderTargetView(RTV, ClearCol);
                D3D11Context->OMSetRenderTargets(1, &RTV, nullptr);
                D3D11Context->Draw(4, 0);
                SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
            }

            tex->Release();
        }
    });

    Session = FramePool.CreateCaptureSession(CaptureItem);
    Session.IsCursorCaptureEnabled(false);

    NullCheck(Session, "WGScreenCaptureRTV: CaptureSession creation failed\n");
}

void WGScreenCaptureRTV::StartSession()
{
    NullCheck(Session, "WGScreenCaptureRTV: Session not initialized before StartSession\n");
    if (Session) {
        Session.StartCapture();
    }
}

void WGScreenCaptureRTV::CloseSession()
{
    if (Session) {
        Session.Close();
        Session = nullptr;
    }
    if (FramePool) {
        FramePool.Close();
        FramePool = nullptr;
    }
    for (auto& Slot : SRVCache) {
        Slot.SurfacePtr = nullptr;
        Slot.TextureView = nullptr;
    }
}

// WGWindowCapture

WGWindowCapture::WGWindowCapture(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11Context_)
{
    D3D11Context = D3D11Context_;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
    }
    SetWrappedD3D11Device(D3D11DevicePtr);
}

WGWindowCapture::~WGWindowCapture()
{
    CloseSession();
}

void WGWindowCapture::CreateWindowCapSession(
    HWND WindowHandle, ID3D11Texture2D* Buffer, UINT Width, UINT Height
)
{
    WBuffer = Buffer;
    TargetHwnd = WindowHandle;

    GetWindowCaptureItem(WindowHandle, CaptureItem);
    NullCheck(CaptureItem, "WGWindowCapture: CaptureItem creation failed for HWND\n");
    if (!CaptureItem)
        return;

    winrt::SizeInt32 ItemSize = CaptureItem.Size();
    CurrentWidth = (Width > 0) ? static_cast<int>(Width) : ItemSize.Width;
    CurrentHeight = (Height > 0) ? static_cast<int>(Height) : ItemSize.Height;

    winrt::SizeInt32 Dimensions{CurrentWidth, CurrentHeight};

    FramePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, Dimensions
    );

    FramePool.FrameArrived([this](auto& Pool, auto&) {
        auto Frame = Pool.TryGetNextFrame();
        if (Frame) {
            auto ContentSize = Frame.ContentSize();
            if (ContentSize.Width > 0 && ContentSize.Height > 0) {
                if (ContentSize.Width != CurrentWidth || ContentSize.Height != CurrentHeight) {
                    CurrentWidth = ContentSize.Width;
                    CurrentHeight = ContentSize.Height;
                    Pool.Recreate(
                        D3DDevice_WGC,
                        winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
                        2,
                        ContentSize
                    );
                }
                std::lock_guard<std::mutex> lock(FrameMutex);
                LatestFrame = std::move(Frame);
                FrameAvailability = true;
            }
        }
    });

    ClosedToken = CaptureItem.Closed([this](auto&, auto&) { CloseSession(); });

    Session = FramePool.CreateCaptureSession(CaptureItem);
    SetCaptureBorderState(Session, false);
    Session.IsCursorCaptureEnabled(false);
    NullCheck(Session, "WGWindowCapture: CaptureSession Creation Failed \n");
}

bool WGWindowCapture::AcquireFrame()
{
    std::lock_guard<std::mutex> lock(FrameMutex);
    if (!FrameAvailability || !LatestFrame)
        return false;

    bool copied = false;
    try {
        auto surface = LatestFrame.Surface();
        auto access =
            surface.as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ComPtr<ID3D11Texture2D> surfaceTex;
        if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&surfaceTex)))) {
            D3D11Context->CopyResource(WBuffer, surfaceTex.Get());
            copied = true;
        }
    } catch (...) {
        Logger::log("WGWindowCapture AcquireFrame: failed to get surface texture\n");
    }

    LatestFrame = nullptr;
    FrameAvailability = false;
    return copied;
}

void WGWindowCapture::StartSession()
{
    NullCheck(Session, "WGWindowCapture: CaptureSession Not Found\n");
    if (Session) {
        Session.StartCapture();
    }
}

void WGWindowCapture::CloseSession()
{
    {
        std::lock_guard<std::mutex> lock(FrameMutex);
        LatestFrame = nullptr;
        FrameAvailability = false;
    }

    if (CaptureItem && ClosedToken) {
        CaptureItem.Closed(ClosedToken);
        ClosedToken = {};
    }

    if (Session) {
        Session.Close();
        Session = nullptr;
    }

    if (FramePool) {
        FramePool.Close();
        FramePool = nullptr;
    }
}

// WGWindowCaptureEx

WGWindowCaptureEx::WGWindowCaptureEx(ID3D11Device* D3D11DevicePtr)
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
    }
    SetWrappedD3D11Device(D3D11DevicePtr);
}

WGWindowCaptureEx::~WGWindowCaptureEx()
{
    CloseSession();
}

void WGWindowCaptureEx::CreateWindowCapSession(
    HWND WindowHandle, UINT Width, UINT Height, FrameCallback OnFrameCallback
)
{
    OnFrameArrived = std::move(OnFrameCallback);
    TargetHWnd = WindowHandle;

    GetWindowCaptureItem(WindowHandle, CaptureItem);
    NullCheck(D3DDevice_WGC, "WGWindowCaptureEx: D3DDevice not set\n");
    NullCheck(CaptureItem, "WGWindowCaptureEx: CaptureItem creation failed for HWND\n");
    if (!CaptureItem)
        return;

    winrt::SizeInt32 ItemSize = CaptureItem.Size();
    CurrentWidth = (Width > 0) ? static_cast<int>(Width) : ItemSize.Width;
    CurrentHeight = (Height > 0) ? static_cast<int>(Height) : ItemSize.Height;

    winrt::SizeInt32 Dims{CurrentWidth, CurrentHeight};

    FramePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 3, Dims
    );
    NullCheck(FramePool, "WGWindowCaptureEx: FramePool creation failed\n");

    FramePool.FrameArrived([this](auto& Pool, auto&) {
        auto frame = Pool.TryGetNextFrame();
        if (!frame)
            return;

        // Minimized window check
        auto contentSize = frame.ContentSize();
        if (contentSize.Width <= 0 || contentSize.Height <= 0)
            return;

        if (contentSize.Width != CurrentWidth || contentSize.Height != CurrentHeight) {
            CurrentWidth = contentSize.Width;
            CurrentHeight = contentSize.Height;
            Pool.Recreate(
                D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 3, contentSize
            );
        }

        auto access =
            frame.Surface()
                .as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ID3D11Texture2D* tex = nullptr;
        if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&tex)))) {
            if (OnFrameArrived) {
                OnFrameArrived(tex);
            }
        }
    });

    ClosedToken = CaptureItem.Closed([this](auto&, auto&) { CloseSession(); });

    Session = FramePool.CreateCaptureSession(CaptureItem);
    SetCaptureBorderState(Session, false);
    Session.IsCursorCaptureEnabled(false);
    NullCheck(Session, "WGWindowCaptureEx: CaptureSession creation failed\n");
}

void WGWindowCaptureEx::CreateWindowCapSession(HWND WindowHandle, FrameCallback OnFrameCallback)
{
    CreateWindowCapSession(WindowHandle, 0, 0, std::move(OnFrameCallback));
}

void WGWindowCaptureEx::StartSession()
{
    NullCheck(Session, "WGWindowCaptureEx: Session not initialized before StartSession\n");
    if (Session) {
        Session.StartCapture();
    }
}

void WGWindowCaptureEx::CloseSession()
{
    OnFrameArrived = nullptr;

    if (CaptureItem && ClosedToken) {
        CaptureItem.Closed(ClosedToken);
        ClosedToken = {};
    }

    if (Session) {
        Session.Close();
        Session = nullptr;
    }
    if (FramePool) {
        FramePool.Close();
        FramePool = nullptr;
    }
}

// WGWindowCaptureRTV

WGWindowCaptureRTV::WGWindowCaptureRTV(
    ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11ContextPtr
)
{
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (...) {
    }
    SetWrappedD3D11Device(D3D11DevicePtr);
    D3D11Device = D3D11DevicePtr;
    D3D11Context = D3D11ContextPtr;
}

WGWindowCaptureRTV::~WGWindowCaptureRTV()
{
    CloseSession();
}

void WGWindowCaptureRTV::ClearSRVCache()
{
    for (auto& Slot : SRVCache) {
        Slot.SurfacePtr = nullptr;
        Slot.TextureView = nullptr;
    }
}

void WGWindowCaptureRTV::CreateWindowCapSession(
    HWND WindowHandle,
    UINT Width,
    UINT Height,
    ID3D11RenderTargetView* RenderTargetView,
    IDXGISwapChain* Swapchain,
    const float ClearColor[4]
)
{
    RTV = RenderTargetView;
    SwapChain = Swapchain;
    if (ClearColor != nullptr) {
        memcpy(ClearCol, ClearColor, sizeof(ClearCol));
    }
    TargetHWnd = WindowHandle;

    GetWindowCaptureItem(WindowHandle, CaptureItem);
    NullCheck(D3DDevice_WGC, "WGWindowCaptureRTV: D3DDevice not set\n");
    NullCheck(CaptureItem, "WGWindowCaptureRTV: CaptureItem creation failed for HWND\n");
    if (!CaptureItem)
        return;

    winrt::SizeInt32 ItemSize = CaptureItem.Size();
    CurrentWidth = (Width > 0) ? static_cast<int>(Width) : ItemSize.Width;
    CurrentHeight = (Height > 0) ? static_cast<int>(Height) : ItemSize.Height;

    winrt::SizeInt32 Dims{CurrentWidth, CurrentHeight};

    FramePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded(
        D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 3, Dims
    );
    NullCheck(FramePool, "WGWindowCaptureRTV: FramePool creation failed\n");

    FramePool.FrameArrived([this](auto& Pool, auto&) {
        auto Frame = Pool.TryGetNextFrame();
        if (!Frame)
            return;

        auto ContentSize = Frame.ContentSize();
        if (ContentSize.Width <= 0 || ContentSize.Height <= 0)
            return;

        if (ContentSize.Width != CurrentWidth || ContentSize.Height != CurrentHeight) {
            CurrentWidth = ContentSize.Width;
            CurrentHeight = ContentSize.Height;
            ClearSRVCache();
            Pool.Recreate(
                D3DDevice_WGC, winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized, 3, ContentSize
            );
        }

        auto access =
            Frame.Surface()
                .as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        ID3D11Texture2D* tex = nullptr;
        if (SUCCEEDED(access->GetInterface(IID_PPV_ARGS(&tex)))) {
            IUnknown* TextureID = nullptr;
            if (SUCCEEDED(
                    tex->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&TextureID))
                )) {
                TextureID->Release();
            }

            ID3D11ShaderResourceView* TextureView = nullptr;

            for (const auto& Slot : SRVCache) {
                if (Slot.SurfacePtr == TextureID && Slot.TextureView != nullptr) {
                    TextureView = Slot.TextureView.Get();
                    break;
                }
            }

            if (TextureView == nullptr) {
                D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
                SrvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
                SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                SrvDesc.Texture2D.MostDetailedMip = 0;
                SrvDesc.Texture2D.MipLevels = 1;

                ComPtr<ID3D11ShaderResourceView> NewTextureView;
                if (SUCCEEDED(D3D11Device->CreateShaderResourceView(
                        tex, &SrvDesc, NewTextureView.GetAddressOf()
                    ))) {
                    TextureView = NewTextureView.Get();

                    for (auto& Slot : SRVCache) {
                        if (Slot.SurfacePtr == nullptr) {
                            Slot.SurfacePtr = TextureID;
                            Slot.TextureView = std::move(NewTextureView);
                            break;
                        }
                    }
                }
            }

            if (TextureView != nullptr && RTV != nullptr && SwapChain != nullptr) {
                D3D11Context->PSSetShaderResources(0, 1, &TextureView);
                D3D11Context->ClearRenderTargetView(RTV, ClearCol);
                D3D11Context->OMSetRenderTargets(1, &RTV, nullptr);
                D3D11Context->Draw(4, 0);
                SwapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
            }

            tex->Release();
        }
    });

    ClosedToken = CaptureItem.Closed([this](auto&, auto&) { CloseSession(); });

    Session = FramePool.CreateCaptureSession(CaptureItem);
    SetCaptureBorderState(Session, false);
    Session.IsCursorCaptureEnabled(false);
    NullCheck(Session, "WGWindowCaptureRTV: CaptureSession creation failed\n");
}

void WGWindowCaptureRTV::StartSession()
{
    NullCheck(Session, "WGWindowCaptureRTV: Session not initialized before StartSession\n");
    if (Session) {
        Session.StartCapture();
    }
}

void WGWindowCaptureRTV::CloseSession()
{
    if (CaptureItem && ClosedToken) {
        CaptureItem.Closed(ClosedToken);
        ClosedToken = {};
    }

    if (Session) {
        Session.Close();
        Session = nullptr;
    }
    if (FramePool) {
        FramePool.Close();
        FramePool = nullptr;
    }
    ClearSRVCache();
}
