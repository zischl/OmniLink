#ifndef NVENCODER_H
#define NVENCODER_H

#include "NvencTypes.h"
#include "OmniLogger.h"
#include <atomic>
#include <cstddef>
#include <d3d11.h>
#include <intrin.h>
#include <nvEncodeAPI.h>
#include <string>
#include <utility>
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

    void RequestKeyframe() { ForceNextIDR.store(true, std::memory_order_relaxed); }
    bool Encode();
    void NVUnlockBitStream();
    void NVCleanup();

  protected:
    NVENCSTATUS Status;
    NV_ENCODE_API_FUNCTION_LIST& NVFunctions;

    NV_ENC_CREATE_BITSTREAM_BUFFER NVOutputBufferDesc = {};

    NvencResourceRegConfig ResourceConfig;

    std::atomic<bool> ForceNextIDR{true};

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
// Just.. ResolveCachedResource before encoding
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

// Takes the Nvidia Encoding sessions to the next level from having single or multiple inputs to
// having multiplee bitstream outputs.
// While this does cost a wee bit of vram this allows zero copy async actions
// Keeps a pool of outputs and each encode marks a slot and locks it down
// Due to safeguards on overwriting data do ReleaseBuffer once ur done with.. uh.. whatever.
template <typename BaseSession> class BufferedNvencSession : public BaseSession
{
  public:
    static constexpr size_t BITSTREAM_POOL_SIZE = 3;
    NV_ENC_OUTPUT_PTR NvencOutputs[BITSTREAM_POOL_SIZE] = {};
    std::atomic<bool> BufferUsageRegistry[BITSTREAM_POOL_SIZE];
    bool BufferLockStates[BITSTREAM_POOL_SIZE] = {};
    NV_ENC_LOCK_BITSTREAM NVBitstreamLocks[BITSTREAM_POOL_SIZE] = {};
    size_t CurrentBufferIndex = 0;
    std::atomic<uint64_t> DroppedFramesCount;

    // Creates extra bitstream buffers.. yes, skipping 0 slot cuz base class handles that shit.
    template <typename... Args>
    BufferedNvencSession(Args&&... args) : BaseSession(std::forward<Args>(args)...)
    {
        NvencOutputs[0] = this->NvencOutput;

        for (size_t i = 0; i < BITSTREAM_POOL_SIZE; ++i) {
            BufferUsageRegistry[i].store(false, std::memory_order_relaxed);
            BufferLockStates[i] = false;
        }

        DroppedFramesCount.store(0, std::memory_order_relaxed);

        for (size_t i = 1; i < BITSTREAM_POOL_SIZE; ++i) {
            NV_ENC_CREATE_BITSTREAM_BUFFER desc = {};
            desc.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
            this->Status = this->NVFunctions.nvEncCreateBitstreamBuffer(this->NVEncoder, &desc);
            if (this->Status != NV_ENC_SUCCESS) {
                Logger::log(("RIP TripleBufferedSession buffer creation " + std::to_string(i) +
                             " : " + std::to_string(this->Status))
                                .c_str());
            }
            NvencOutputs[i] = desc.bitstreamBuffer;
        }
    }

    // Cleans up extra bitstreams, Base class already handles cleanup for the first one...
    ~BufferedNvencSession()
    {
        for (size_t i = 1; i < BITSTREAM_POOL_SIZE; ++i) {
            if (BufferLockStates[i]) {
                this->NVFunctions.nvEncUnlockBitstream(this->NVEncoder, NvencOutputs[i]);
                BufferLockStates[i] = false;
            }
            if (NvencOutputs[i]) {
                this->NVFunctions.nvEncDestroyBitstreamBuffer(this->NVEncoder, NvencOutputs[i]);
                NvencOutputs[i] = nullptr;
            }
        }
    }

    size_t GetLastEncodedSlotIndex() const
    {
        return (CurrentBufferIndex == 0) ? (BITSTREAM_POOL_SIZE - 1) : (CurrentBufferIndex - 1);
    }

    void ReleaseBuffer(size_t slotIndex)
    {
        BufferUsageRegistry[slotIndex].store(false, std::memory_order_release);
    }

    // Same as base class Encode functionality with the addition of UsageRegistry and LockStates
    // Auto locks bitstream, advances pool, auto unlocks if not busy.
    // Just.. do remember to ReleaseBuffer
    bool Encode()
    {
        if (BufferUsageRegistry[CurrentBufferIndex].load(std::memory_order_acquire)) {
            DroppedFramesCount.fetch_add(1, std::memory_order_relaxed);
            Logger::log("Excuse me, You're holding on to the Bitstream buffers too long.. \n");
            return false;
        }

        if (BufferLockStates[CurrentBufferIndex]) {
            this->NVFunctions.nvEncUnlockBitstream(
                this->NVEncoder, NvencOutputs[CurrentBufferIndex]
            );
            BufferLockStates[CurrentBufferIndex] = false;
        }

        // Map input resource
        NV_ENC_MAP_INPUT_RESOURCE NVInputResource = {};
        NVInputResource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
        NVInputResource.registeredResource = this->NVRegisterResource.registeredResource;
        this->Status = this->NVFunctions.nvEncMapInputResource(this->NVEncoder, &NVInputResource);
        if (this->Status != NV_ENC_SUCCESS) {
            Logger::log(this->NVFunctions.nvEncGetLastErrorString(this->NVEncoder));
            Logger::log(("RIP Input Resource Map \n" + std::to_string(this->Status)).c_str());
            return false;
        }

        bool ForceIDR = this->ForceNextIDR.exchange(false, std::memory_order_relaxed);

        NV_ENC_PIC_PARAMS NvencPicParams = {};
        memset(&NvencPicParams, 0, sizeof(NV_ENC_PIC_PARAMS));
        NvencPicParams.version = NV_ENC_PIC_PARAMS_VER;
        NvencPicParams.inputBuffer = NVInputResource.mappedResource;
        NvencPicParams.bufferFmt = NVInputResource.mappedBufferFmt;
        NvencPicParams.outputBitstream = NvencOutputs[CurrentBufferIndex];
        NvencPicParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
        NvencPicParams.encodePicFlags = ForceIDR ? NV_ENC_PIC_FLAG_FORCEIDR : 0;
        NvencPicParams.completionEvent = nullptr;

        this->Status = this->NVFunctions.nvEncEncodePicture(this->NVEncoder, &NvencPicParams);
        this->NVFunctions.nvEncUnmapInputResource(this->NVEncoder, NVInputResource.mappedResource);
        if (this->Status != NV_ENC_SUCCESS) {
            Logger::log(this->NVFunctions.nvEncGetLastErrorString(this->NVEncoder));
            Logger::log(("\n RIP Encoding " + std::to_string(this->Status)).c_str());
            return false;
        }

        NVBitstreamLocks[CurrentBufferIndex] = {};
        NVBitstreamLocks[CurrentBufferIndex].version = NV_ENC_LOCK_BITSTREAM_VER;
        NVBitstreamLocks[CurrentBufferIndex].outputBitstream = NvencOutputs[CurrentBufferIndex];
        NVBitstreamLocks[CurrentBufferIndex].doNotWait = 0;

        this->Status = this->NVFunctions.nvEncLockBitstream(
            this->NVEncoder, &NVBitstreamLocks[CurrentBufferIndex]
        );
        if (this->Status != NV_ENC_SUCCESS) {
            Logger::log(this->NVFunctions.nvEncGetLastErrorString(this->NVEncoder));
            Logger::log(("\n RIP Output Lock " + std::to_string(this->Status)).c_str());
            return false;
        }

        BufferLockStates[CurrentBufferIndex] = true;
        BufferUsageRegistry[CurrentBufferIndex].store(true, std::memory_order_release);

        this->NVBitstreamLock = NVBitstreamLocks[CurrentBufferIndex];

        CurrentBufferIndex = (CurrentBufferIndex + 1) % BITSTREAM_POOL_SIZE;

        return true;
    }
};
#endif
