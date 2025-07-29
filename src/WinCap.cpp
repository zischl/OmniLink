#include "WinCap.h"

ComPtr<IDXGIOutputDuplication> DXGICapture::InitDXGI(ComPtr<ID3D11Device> D3D11Device) {
	ComPtr<IDXGIDevice> DXGIDevice;
	D3D11Device.As(&DXGIDevice);

	ComPtr<IDXGIAdapter> DXGIAdapter;
	DXGIDevice->GetAdapter(&DXGIAdapter);

	ComPtr<IDXGIOutput> DXGIOutput;
	DXGIAdapter->EnumOutputs(0, &DXGIOutput);

	ComPtr<IDXGIOutput1> DXGIOutputEnhanced;
	DXGIOutput.As(&DXGIOutputEnhanced);

	ComPtr<IDXGIOutputDuplication> DXGIOutDuplication;
	DXGIOutputEnhanced->DuplicateOutput(D3D11Device.Get(), &DXGIOutDuplication);

	return DXGIOutDuplication;

}



//void DXGICapture::CaptureDXGI(IDXGIOutputDuplication* DXGIOutDuplication, ComPtr<ID3D11Texture2D>& texture2d) 