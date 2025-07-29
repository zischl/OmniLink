
#ifndef WINCAP_H
#define WINCAP_H

#pragma once

#include <d3d11.h>
#include <dxgi1_5.h>
#include <Windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class DXGICapture {
private:
	HRESULT hr;

public:
	ComPtr<IDXGIOutputDuplication> InitDXGI(ComPtr<ID3D11Device> D3D11Device);

};

inline void CaptureDXGI(IDXGIOutputDuplication* DXGIOutDuplication, ComPtr<ID3D11Texture2D>& texture2d) {
	DXGI_OUTDUPL_FRAME_INFO frameinfo;
	ComPtr<IDXGIResource> framepixeldata;

	DXGIOutDuplication->AcquireNextFrame(1000, &frameinfo, &framepixeldata);

	framepixeldata.As(&texture2d);
}

#endif