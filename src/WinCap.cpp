#include "WinCap.h"

#define NullCheck(item, text)  { \
	if (item == nullptr) { \
		Logger::log(text);  \
	}					   \
}

ComPtr<IDXGIOutputDuplication> DXGICapture::InitDXGI(ComPtr<ID3D11Device> D3D11Device) {
	ComPtr<IDXGIDevice> DXGIDevice;
	D3D11Device.As(&DXGIDevice);

	ComPtr<IDXGIAdapter> DXGIAdapter;
	DXGIDevice->GetAdapter(&DXGIAdapter);

	ComPtr<IDXGIOutput> DXGIOutput;
	DXGIAdapter->EnumOutputs(0, &DXGIOutput);

	ComPtr<IDXGIOutput1> DXGIOutputEnhanced;
	DXGIOutput.As(&DXGIOutputEnhanced);

	DXGIOutputEnhanced->DuplicateOutput(D3D11Device.Get(), &DXGIOutDuplication);

	DXGIOutDuplication->AcquireNextFrame(1000, &frameinfo, &framepixeldata);
	framepixeldata.As(&DXGIComBuffer);

	return DXGIOutDuplication;

}

ID3D11Texture2D* DXGICapture::GetBuffer() const { return DXGIComBuffer.Get(); }


//void DXGICapture::CaptureDXGI(IDXGIOutputDuplication* DXGIOutDuplication, ComPtr<ID3D11Texture2D>& texture2d) 


namespace winrt
{
	using namespace Windows::Graphics;
	using namespace Windows::Graphics::Capture;
	using namespace Windows::Graphics::DirectX;
	using namespace Windows::Graphics::DirectX::Direct3D11;
}




void WGCapture::GetActiveMonitorCaptureItem(
	winrt::Windows::Graphics::Capture::GraphicsCaptureItem& CaptureItem
) {

	winrt::com_ptr<IGraphicsCaptureItemInterop> WGCInterop;

	GetRoActivationFactory(WGCInterop.put_void());
	NullCheck(WGCInterop.get(), "WGC Item Interop Get Failed for Monitor Capture\n");

	winrt::check_hresult
	(
		WGCInterop->CreateForMonitor
		(
			GetActiveMonitor(),
			winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
			winrt::put_abi(CaptureItem)
		)
	);


}

WGScreenCapture::WGScreenCapture() {
}




void WGScreenCapture::InitWGC(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11Context_, ID3D11Texture2D* Buffer, UINT Width, UINT Height) {
	WBuffer = Buffer;
	D3D11Context = D3D11Context_;

	winrt::init_apartment(winrt::apartment_type::multi_threaded);

	winrt::com_ptr<IGraphicsCaptureItemInterop> WGCInterop;

	GetRoActivationFactory(WGCInterop.put_void());

	SetWrappedD3D11Device(D3D11DevicePtr);

	GetActiveMonitorCaptureItem(CaptureItem);

	winrt::SizeInt32 Dimensions;
	Dimensions.Width = Width;
	Dimensions.Height = Height;

	FramePool = winrt::Direct3D11CaptureFramePool::CreateFreeThreaded
	(
		D3DDevice_WGC,
		winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
		2,
		Dimensions
	);



	FramePool.FrameArrived([this](auto Pool, auto a) {
		winrt::Direct3D11CaptureFrame ValidFrame{ nullptr };
		while (true) {
			winrt::Direct3D11CaptureFrame Frame = Pool.TryGetNextFrame();
			if (!Frame) {
				break;
			}
			ValidFrame = Frame;
		}

		if (!WriteState.load()) {
			_SurfaceInterface = ValidFrame.Surface().as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
			_SurfaceInterface->GetInterface(IID_PPV_ARGS(&_SurfaceTexture));
			D3D11Context->CopyResource(WBuffer, _SurfaceTexture);
		}

		}
	);

	NullCheck(D3DDevice_WGC, "D3DDevice Not Set\n")
	NullCheck(FramePool, "WGC FramePool Creation Failed\n");
	NullCheck(CaptureItem, "WGC Capture Item Creation Failed\n");
	NullCheck(WBuffer, "Write Buffer Not Set\n");
	

	if (CaptureItem != nullptr) {
		Session = FramePool.CreateCaptureSession(CaptureItem);
		Session.IsCursorCaptureEnabled(false);
		NullCheck(Session, "CaptureSession Creation Failed \n");
		Session.StartCapture();
	}

}