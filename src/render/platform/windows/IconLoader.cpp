#include "IconLoader.h"

bool IconLoader::LoadEmbeddedRGBA(const unsigned char* bytes,
                                  unsigned int width,
                                  unsigned int height,
                                  ID3D11Device* D3DDevice)
{
    Release();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = bytes;
    initData.SysMemPitch = width * 4;

    ID3D11Texture2D* IconTexture = nullptr;
    if (FAILED(D3DDevice->CreateTexture2D(&desc, &initData, &IconTexture)))
        return false;

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = D3DDevice->CreateShaderResourceView(IconTexture, nullptr, &srv);
    IconTexture->Release();

    IconTextureHandle = static_cast<void*>(srv);
    return SUCCEEDED(hr);
}

void IconLoader::Release()
{
    if (!IconTextureHandle)
        return;

    auto* srv = static_cast<ID3D11ShaderResourceView*>(IconTextureHandle);
    srv->Release();
    IconTextureHandle = nullptr;
}
