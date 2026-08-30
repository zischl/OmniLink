#ifndef NVDECODER_H
#define NVDECODER_H

#pragma once
#include "CuKernel.cuh"

#include <cuviddec.h>
#include <d3d11.h>
#include <fstream>
#include <string>
#include <windows.h>

#include <cuda.h>
#include <cudaD3D11.h>
#define _TIMECODE NV_TIMECODE
#define TIMECODE NV_TIMECODE_STRUCT
#include <nvcuvid.h>
#undef _TIMECODE
#undef TIMECODE

#pragma comment(lib, "nvcuvid.lib")
// #pragma comment(lib, "cuda.lib")

typedef CUresult CUDAAPI tcuvidCreateVideoParser(CUvideoparser* pObj, CUVIDPARSERPARAMS* pParams);
typedef CUresult CUDAAPI tcuvidParseVideoData(CUvideoparser obj, CUVIDSOURCEDATAPACKET* pPacket);
typedef CUresult CUDAAPI tcuvidDestroyVideoParser(CUvideoparser obj);

typedef CUresult CUDAAPI tcuvidGetDecoderCaps(CUVIDDECODECAPS* pdc);
typedef CUresult CUDAAPI
tcuvidCreateDecoder(CUvideodecoder* phDecoder, CUVIDDECODECREATEINFO* pdci);
typedef CUresult CUDAAPI tcuvidDestroyDecoder(CUvideodecoder hDecoder);
typedef CUresult CUDAAPI tcuvidDecodePicture(CUvideodecoder hDecoder, CUVIDPICPARAMS* pPicParams);
typedef CUresult CUDAAPI
tcuvidReconfigureDecoder(CUvideodecoder hDecoder, CUVIDRECONFIGUREDECODERINFO* pDecReconfigParams);
typedef CUresult CUDAAPI tcuvidMapVideoFrame64(
    CUvideodecoder hDecoder,
    int nPicIdx,
    unsigned long long* pDevPtr,
    unsigned int* pPitch,
    CUVIDPROCPARAMS* pVPP
);
typedef CUresult CUDAAPI
tcuvidUnmapVideoFrame64(CUvideodecoder hDecoder, unsigned long long DevPtr);

typedef CUresult CUDAAPI tcuGraphicsD3D11RegisterResource(
    CUgraphicsResource* pCudaResource, ID3D11Resource* pD3DResource, unsigned int Flags
);
typedef CUresult CUDAAPI
tcuGraphicsMapResources(unsigned int count, CUgraphicsResource* resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsSubResourceGetMappedArray(
    CUarray* pArray, CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel
);
typedef CUresult CUDAAPI
tcuGraphicsUnmapResources(unsigned int count, CUgraphicsResource* resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsUnregisterResource(CUgraphicsResource resource);

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define GET_PROC(name, dll)                                                                        \
    name = reinterpret_cast<t##name*>(GetProcAddress(dll, #name));                                 \
    if (!name) {                                                                                   \
        OutputDebugString("Get Proc Failed");                                                      \
        CResult = CUDA_ERROR_UNKNOWN;                                                              \
        return CUDA_ERROR_UNKNOWN;                                                                 \
    }

#endif

class NVDecoder
{
  public:
    virtual ~NVDecoder() = default;

    static CUresult Initialize();
    static void Release();
    static void CompatibilityCheck();

    static tcuvidCreateVideoParser* cuvidCreateVideoParser;
    static tcuvidParseVideoData* cuvidParseVideoData;
    static tcuvidDestroyVideoParser* cuvidDestroyVideoParser;

    static tcuvidGetDecoderCaps* cuvidGetDecoderCaps;
    static tcuvidCreateDecoder* cuvidCreateDecoder;
    static tcuvidDestroyDecoder* cuvidDestroyDecoder;
    static tcuvidDecodePicture* cuvidDecodePicture;
    static tcuvidReconfigureDecoder* cuvidReconfigureDecoder;
    static tcuvidMapVideoFrame64* cuvidMapVideoFrame64;
    static tcuvidUnmapVideoFrame64* cuvidUnmapVideoFrame64;

    static tcuGraphicsD3D11RegisterResource* cuGraphicsD3D11RegisterResource;
    static tcuGraphicsMapResources* cuGraphicsMapResources;
    static tcuGraphicsSubResourceGetMappedArray* cuGraphicsSubResourceGetMappedArray;
    static tcuGraphicsUnmapResources* cuGraphicsUnmapResources;
    static tcuGraphicsUnregisterResource* cuGraphicsUnregisterResource;

  protected:
    static CUresult CResult;
    static HMODULE NvDecAPIHandle;
    static HMODULE NvCudaAPIHandle;

    static CUcontext CudaContext;
    static CUdevice CudaDevice;
    static bool Initialized;
};

// Nvidia Decoder Sessions
// Context and functions will be automatically Initialized if not done
// Sessions after the creation of the first session will reuse that context
// Context can be manually initialized with the base class NVDecoder
// CloseSession to clean the session and NVDecoder::Release to.. well.. release context
class NvdecSession : public NVDecoder
{
  public:
    NvdecSession(UINT Width, UINT Height, ID3D11Texture2D* OutputTexture);
    ~NvdecSession();

    CUresult InitializeSession();
    void Decode(const unsigned char* data, unsigned long size);
    void CloseSession();

  private:
    UINT Width;
    UINT Height;
    ID3D11Texture2D* OutputBuffer;
    CUgraphicsResource CudaOutputResource;

    CUvideoparser CudaParser;
    CUVIDPARSERPARAMS CudaParserParams = {};

    CUvideodecoder CudaDecoder;
    CUVIDDECODECREATEINFO CudaDecoderInfo = {};

    CUresult SessionResult;

    static int CUDAAPI ParserSequenceCallback(void* instanceData, CUVIDEOFORMAT* CuDecoderInfo);
    static int CUDAAPI PictureDecodeCallback(void* instanceData, CUVIDPICPARAMS* pPicParams);
    static int CUDAAPI PictureOutputCallback(void* instanceData, CUVIDPARSERDISPINFO* pDispInfo);
};

#endif
