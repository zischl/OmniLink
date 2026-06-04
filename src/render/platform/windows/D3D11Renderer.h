#ifndef OMNIRENDERER_H
#define OMNIRENDERER_H

#pragma once

#include <D3D11RendererCore.h>

struct HWNDxD3D11
{
    ComPtr<ID3D11Device> D3D11Device = nullptr;
    ComPtr<ID3D11DeviceContext> D3D11Context = nullptr;
    ComPtr<IDXGISwapChain3> swapchain = nullptr;
    ComPtr<ID3D11RenderTargetView> renderTargetView = nullptr;
};

struct HWNDxShaders
{
    ComPtr<ID3D11PixelShader> pixelShader = nullptr;
    ComPtr<ID3D11VertexShader> vertexShader = nullptr;
    ComPtr<ID3D11Buffer> vertexBuffer = nullptr;
    UINT VertexBufferStride = 0;
    UINT VertexBufferOffset = 0;
    ComPtr<ID3D11InputLayout> inputLayout = nullptr;
    ComPtr<ID3D11Buffer> IndexBuffer = nullptr;
    ComPtr<ID3D11SamplerState> sampler = nullptr;
};

class D3D11Renderer : public Renderer
{
  public:
    void RendererInit(HWND hwnd, int width, int height, HWNDxD3D11& RendererPtrStruct);

    HWNDxShaders ShadersInit(ID3D11Device* D3D11Device);

    void SetShaders(ID3D11DeviceContext* D3D11Context, HWNDxShaders* shaders);
};

struct D3DState
{
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* Context = nullptr;
    IDXGISwapChain3* Swapchain = nullptr;
    ID3D11RenderTargetView* RTV = nullptr;
};

#endif
