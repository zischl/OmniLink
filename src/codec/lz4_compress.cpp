
#ifdef _WIN32

#include <d3d11.h>
#include <debugapi.h>
#include <lz4.h>
#include <string>
#include <thread>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class LZ4TextureCompressor
{
  public:
    static HRESULT staging_texture_for_compression(ID3D11Device* D3D11Device,
                                                   ID3D11Texture2D** stagingTexture,
                                                   UINT width,
                                                   UINT height,
                                                   DXGI_FORMAT format)
    {
        if (!D3D11Device || !stagingTexture)
            return E_INVALIDARG;

        D3D11_TEXTURE2D_DESC stagingBufferDesc = {};
        stagingBufferDesc.Width = width;
        stagingBufferDesc.Height = height;
        stagingBufferDesc.Format = format;
        stagingBufferDesc.Usage = D3D11_USAGE_STAGING;
        stagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingBufferDesc.BindFlags = 0;
        stagingBufferDesc.SampleDesc.Count = 1;
        stagingBufferDesc.SampleDesc.Quality = 0;
        stagingBufferDesc.ArraySize = 1;
        stagingBufferDesc.MipLevels = 1;
        stagingBufferDesc.MiscFlags = 0;

        return D3D11Device->CreateTexture2D(&stagingBufferDesc, nullptr, stagingTexture);
    }

    static void lz4_compression(ID3D11DeviceContext* D3D11Context,
                                ID3D11Texture2D* stagingTexture,
                                ID3D11Texture2D* mainBuffer,
                                unsigned int width,
                                unsigned int height)
    {
        if (!D3D11Context || !stagingTexture || !mainBuffer)
            return;

        D3D11Context->CopyResource(stagingTexture, mainBuffer);

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = D3D11Context->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);
        if (FAILED(hr)) {
            OutputDebugStringW(L"Failed to map staging texture.\n");
            return;
        }

        BYTE* pixelResource = static_cast<BYTE*>(mappedResource.pData);
        UINT rowPitch = mappedResource.RowPitch;

        UINT TotalChunkedBytes = (rowPitch * height) / 2;

        std::thread t1([pixelResource, rowPitch, TotalChunkedBytes] {
            std::vector<BYTE> chunk1(TotalChunkedBytes);
            memcpy(chunk1.data(), pixelResource, TotalChunkedBytes);

            int maxCompressedSize = LZ4_compressBound(TotalChunkedBytes);
            std::vector<char> compressedBuffer(maxCompressedSize);
            int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(chunk1.data()),
                                                      compressedBuffer.data(),
                                                      TotalChunkedBytes,
                                                      maxCompressedSize);
            if (compressedSize <= 0) {
                OutputDebugStringW(L"Thread 1 FAILED\n");
            } else {
                OutputDebugStringW(
                    (L"Thread 1 Compressed Size: " + std::to_wstring(compressedSize) + L" aaa\n")
                        .c_str());
            }
        });

        std::thread t2([pixelResource, height, rowPitch, TotalChunkedBytes] {
            std::vector<BYTE> chunk2(TotalChunkedBytes);
            memcpy(chunk2.data(), pixelResource + ((height / 2) * rowPitch), TotalChunkedBytes);

            int maxCompressedSize = LZ4_compressBound(TotalChunkedBytes);
            std::vector<char> compressedBuffer(maxCompressedSize);
            int compressedSize = LZ4_compress_default(reinterpret_cast<const char*>(chunk2.data()),
                                                      compressedBuffer.data(),
                                                      TotalChunkedBytes,
                                                      maxCompressedSize);
            if (compressedSize <= 0) {
                OutputDebugStringW(L"Thread 2 FAILED\n");
            } else {
                OutputDebugStringW(
                    (L"Thread 2 Compressed Size: " + std::to_wstring(compressedSize) + L" bbb\n")
                        .c_str());
            }
        });

        t1.join();
        t2.join();

        D3D11Context->Unmap(stagingTexture, 0);
    }
};

#endif
