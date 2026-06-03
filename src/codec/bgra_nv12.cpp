#ifdef _WIN32

#include <d3d11.h>
#include <d3dcompiler.h>
#include <debugapi.h>
#include <string>
#include <wrl/client.h>

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;

class VideoConverter
{
  public:
    struct NV12Resources
    {
        ComPtr<ID3D11Texture2D> PlaneYTexture;
        ComPtr<ID3D11Texture2D> PlaneUVTexture;
        ComPtr<ID3D11UnorderedAccessView> UAViewY;
        ComPtr<ID3D11UnorderedAccessView> UAViewUV;
        ComPtr<ID3D11ComputeShader> NV12ComputeShader;
    };

    static HRESULT PrepareBGRA2NV12Compute(ID3D11Device* D3D11Device,
                                           UINT width,
                                           UINT height,
                                           const std::wstring& shaderPath,
                                           NV12Resources& outResources)
    {
        if (!D3D11Device)
            return E_INVALIDARG;

        HRESULT hr = S_OK;

        D3D11_TEXTURE2D_DESC YTextureDesc = {};
        YTextureDesc.Width = width;
        YTextureDesc.Height = height;
        YTextureDesc.Format = DXGI_FORMAT_R8_UNORM;
        YTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        YTextureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        YTextureDesc.SampleDesc.Count = 1;
        YTextureDesc.ArraySize = 1;
        YTextureDesc.MipLevels = 1;

        hr = D3D11Device->CreateTexture2D(
            &YTextureDesc, nullptr, outResources.PlaneYTexture.ReleaseAndGetAddressOf());
        if (FAILED(hr))
            return hr;

        D3D11_TEXTURE2D_DESC UVTextureDesc = {};
        UVTextureDesc.Width = width / 2;
        UVTextureDesc.Height = height / 2;
        UVTextureDesc.Format = DXGI_FORMAT_R8G8_UNORM;
        UVTextureDesc.Usage = D3D11_USAGE_DEFAULT;
        UVTextureDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
        UVTextureDesc.SampleDesc.Count = 1;
        UVTextureDesc.ArraySize = 1;
        UVTextureDesc.MipLevels = 1;

        hr = D3D11Device->CreateTexture2D(
            &UVTextureDesc, nullptr, outResources.PlaneUVTexture.ReleaseAndGetAddressOf());
        if (FAILED(hr))
            return hr;

        D3D11_UNORDERED_ACCESS_VIEW_DESC UAViewYDesc = {};
        UAViewYDesc.Format = YTextureDesc.Format;
        UAViewYDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        UAViewYDesc.Texture2D.MipSlice = 0;

        hr = D3D11Device->CreateUnorderedAccessView(outResources.PlaneYTexture.Get(),
                                                    &UAViewYDesc,
                                                    outResources.UAViewY.ReleaseAndGetAddressOf());
        if (FAILED(hr))
            return hr;

        D3D11_UNORDERED_ACCESS_VIEW_DESC UAViewUVDesc = {};
        UAViewUVDesc.Format = UVTextureDesc.Format;
        UAViewUVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        UAViewUVDesc.Texture2D.MipSlice = 0;

        hr = D3D11Device->CreateUnorderedAccessView(outResources.PlaneUVTexture.Get(),
                                                    &UAViewUVDesc,
                                                    outResources.UAViewUV.ReleaseAndGetAddressOf());
        if (FAILED(hr))
            return hr;

        ComPtr<ID3DBlob> computeShaderBlob;
        hr = D3DReadFileToBlob(shaderPath.c_str(), &computeShaderBlob);
        if (FAILED(hr)) {
            OutputDebugStringW(
                (L"BGRA2NV12Shader Read File Failed: " + shaderPath + L"\n").c_str());
            return hr;
        }

        hr = D3D11Device->CreateComputeShader(
            computeShaderBlob->GetBufferPointer(),
            computeShaderBlob->GetBufferSize(),
            nullptr,
            outResources.NV12ComputeShader.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            OutputDebugStringW(L"Compute Shader Creation Failed.\n");
            return hr;
        }

        return S_OK;
    }

    static void RunBGRA2NV12Compute(ID3D11DeviceContext* D3D11Context,
                                    const NV12Resources& resources,
                                    ID3D11ShaderResourceView* sourceBgraSRV,
                                    UINT width,
                                    UINT height)
    {
        if (!D3D11Context || !sourceBgraSRV)
            return;

        D3D11Context->CSSetShader(resources.NV12ComputeShader.Get(), nullptr, 0);

        D3D11Context->CSSetShaderResources(0, 1, &sourceBgraSRV);

        ID3D11UnorderedAccessView* UAViewsYUV[] = {resources.UAViewY.Get(),
                                                   resources.UAViewUV.Get()};
        D3D11Context->CSSetUnorderedAccessViews(0, 2, UAViewsYUV, nullptr);

        UINT threadGroupX = (width + 15) / 16;
        UINT threadGroupY = (height + 15) / 16;
        D3D11Context->Dispatch(threadGroupX, threadGroupY, 1);

        ID3D11ShaderResourceView* nullSRV[1] = {nullptr};
        D3D11Context->CSSetShaderResources(0, 1, nullSRV);

        ID3D11UnorderedAccessView* nullUAV[2] = {nullptr, nullptr};
        D3D11Context->CSSetUnorderedAccessViews(0, 2, nullUAV, nullptr);

        D3D11Context->CSSetShader(nullptr, nullptr, 0);
    }
};

#endif
