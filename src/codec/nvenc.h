#ifndef NVENCODER_H
#define NVENCODER_H

#include "NvencTypes.h"
#include "OmniLogger.h"
#include <d3d11.h>
#include <nvEncodeAPI.h>
#include <string>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

#pragma comment(lib, "nvencodeapi.lib")

class NVENCODER
{
  private:
    NVENCSTATUS Status;

    size_t* OutputSize;
    uint8_t* Output;

    bool EncodeStatus = false;

  public:
    NV_ENCODE_API_FUNCTION_LIST NVFunctions = {};

    NVENCODER();
    ~NVENCODER();

    void LoadNvEncodeAPI();

    static void GetSupportedCodecGUIDs(void* NVEncoder, NV_ENCODE_API_FUNCTION_LIST& NVFunctions_);
    void GetAvailablePresetGUIDs(void* NVEncoder);
    void GetAvailableProfileGUIDs(void* NVEncoder, GUID NvencCodecGUID);
    void GetSupportedInputFormats(void* NVEncoder, GUID NvencCodecGUID);
};

// Nvidia encoding session base class.
// Enough for on the fly resource management (Register and Unregister per Frame)
// Shitty performance compared to Static Allocation or Cached pool tho
// To be exact it's like 10 us vs maybe 2 ns for cache lookup but still, shit
class NvencSession
{
  public:
    void* NVEncoder = nullptr;

    NV_ENC_REGISTER_RESOURCE NVRegisterResource = {};
    NV_ENC_OUTPUT_PTR NvencOutput = nullptr;
    NV_ENC_LOCK_BITSTREAM NVBitstreamLock = {};

    NvencSession(
        void* D3DDevice,
        NV_ENCODE_API_FUNCTION_LIST& NVFunctions_,
        UINT EncodeWidth,
        UINT EncodeHeight
    );
    ~NvencSession();

    // Direct resource management helpers
    NV_ENC_REGISTERED_PTR
    RegisterResource(ID3D11Texture2D* inputResource, const NvencResourceRegConfig& config);

    void UnregisterResource(NV_ENC_REGISTERED_PTR registeredHandle);

    NV_ENC_REGISTERED_PTR SwapResource(
        NV_ENC_REGISTERED_PTR oldHandle,
        ID3D11Texture2D* newTex,
        const NvencResourceRegConfig& config
    );

    void Encode();
    void NVUnlockBitStream();
    void NVCleanup();

  protected:
    NVENCSTATUS Status;
    NV_ENCODE_API_FUNCTION_LIST& NVFunctions;

    NV_ENC_CREATE_BITSTREAM_BUFFER NVOutputBufferDesc = {};

    NvencResourceRegConfig ResourceConfig;

    void OpenNvEncSession(void* D3DDevice);
    void LoadDefaultInitParams(NV_ENC_INITIALIZE_PARAMS& NvInitParams, NvencInitConfig& config);
    void NVEncoderInit(NV_ENC_INITIALIZE_PARAMS& NvInitParams);
    void CreateBitStream();

    NV_ENC_OUTPUT_PTR getBitstream() const { return NvencOutput; }
    NV_ENC_REGISTER_RESOURCE getRegisteredResource() const { return NVRegisterResource; }
};

// Nvidia encoding session Static Allocation
// Registers texture resource only once, reuses it throughout the session
class StaticNvencSession : public NvencSession
{
  public:
    StaticNvencSession(
        void* D3DDevice,
        NV_ENCODE_API_FUNCTION_LIST& NVFunctions_,
        ID3D11Texture2D* InputResource,
        UINT EncodeWidth,
        UINT EncodeHeight
    );
    ~StaticNvencSession();

    void NVCleanup();
};

// Nvidia encoding session extended with a Cached Resource Pool
// Perfect for Texture Pools
class CachedPoolNvencSession : public NvencSession
{
  public:
    struct CachedNvencSlot
    {
        IUnknown* SurfaceRawPtr = nullptr;
        ComPtr<ID3D11Texture2D> D3DTexture;
        NV_ENC_REGISTERED_PTR NvRegisteredHandle = nullptr;
    };

    CachedPoolNvencSession(
        void* D3DDevice,
        NV_ENCODE_API_FUNCTION_LIST& NVFunctions_,
        UINT EncodeWidth,
        UINT EncodeHeight,
        size_t PoolSize = 3
    );
    ~CachedPoolNvencSession();

    void ResolveCachedResource(ID3D11Texture2D* NewTex);
    void NVCleanup();

  private:
    std::vector<CachedNvencSlot> PoolCache;
};
#endif
