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
        DXGIOutDuplication->ReleaseFrame();
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

    hr = DXGIOutDuplication->AcquireNextFrame(33, &FrameInfo, &FramePixelData);
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
