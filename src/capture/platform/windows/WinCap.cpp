#include "WinCap.h"

#define NullCheck(item, text)  { \
	if (item == nullptr) { \
		Logger::log(text);  \
	}					   \
}

ComPtr<IDXGIOutputDuplication> DXGICapture::InitDXGI(ID3D11Device* D3D11Device) {


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

	DXGIOutDuplication->AcquireNextFrame(0, &frameinfo, &framepixeldata);
	framepixeldata.As(&DXGIComBuffer);

	DXGIOutDuplication->ReleaseFrame();
		
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

void WGCapture::CreateWGCBuffer(ID3D11Device* D3D11Device, ID3D11Texture2D** Buffer) {
	D3D11_TEXTURE2D_DESC custommainBufferDesc = {};
	custommainBufferDesc.Width = 1920;
	custommainBufferDesc.Height = 1080;
	custommainBufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	custommainBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	custommainBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	custommainBufferDesc.SampleDesc.Count = 1;
	custommainBufferDesc.SampleDesc.Quality = 0;
	custommainBufferDesc.ArraySize = 1;
	custommainBufferDesc.MipLevels = 1;
	custommainBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

	HRESULT hr = D3D11Device->CreateTexture2D(&custommainBufferDesc, nullptr, Buffer);
	if (FAILED(hr)) {
		Logger::log((std::to_string(hr) + "WGC Output Buffer Creation Failed\n").c_str());
	}
}


WGScreenCapture::WGScreenCapture(ID3D11Device* D3D11DevicePtr, ID3D11DeviceContext* D3D11Context_) {
	D3D11Context = D3D11Context_;

	
	winrt::init_apartment(winrt::apartment_type::multi_threaded);

	winrt::com_ptr<IGraphicsCaptureItemInterop> WGCInterop;

	GetRoActivationFactory(WGCInterop.put_void());

	SetWrappedD3D11Device(D3D11DevicePtr);

}




void WGScreenCapture::CreateMonitorCapSession(ID3D11Texture2D* Buffer, UINT Width, UINT Height) {
	WBuffer = Buffer;

	
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



	FramePool.FrameArrived([this](auto &Pool, auto &a) {
		
		while (true) {
			Frame = Pool.TryGetNextFrame();
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
		
	}

}

void WGScreenCapture::StartSession() {
	NullCheck(Session, "CaptureSession Not Found\n");
	Session.StartCapture();
}

void WGScreenCapture::CloseSession() {
	Session.Close();
}


