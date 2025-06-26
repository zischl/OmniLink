#ifndef NVDECODER_H
#define NVDECODER_H

#pragma once
#include "CuKernel.cuh"

#include <fstream>
#include <string>
#include <windows.h>

#include <cuda.h>
#include <d3d11.h>
#include <cudaD3D11.h>
#include <cuviddec.h>
#include <nvcuvid.h>

#pragma comment(lib, "nvcuvid.lib")
//#pragma comment(lib, "cuda.lib")


typedef CUresult CUDAAPI tcuvidCreateVideoParser(CUvideoparser* pObj, CUVIDPARSERPARAMS* pParams);
typedef CUresult CUDAAPI tcuvidParseVideoData(CUvideoparser obj, CUVIDSOURCEDATAPACKET* pPacket);
typedef CUresult CUDAAPI tcuvidDestroyVideoParser(CUvideoparser obj);

typedef CUresult CUDAAPI tcuvidGetDecoderCaps(CUVIDDECODECAPS* pdc);
typedef CUresult CUDAAPI tcuvidCreateDecoder(CUvideodecoder* phDecoder, CUVIDDECODECREATEINFO* pdci);
typedef CUresult CUDAAPI tcuvidDestroyDecoder(CUvideodecoder hDecoder);
typedef CUresult CUDAAPI tcuvidDecodePicture(CUvideodecoder hDecoder, CUVIDPICPARAMS* pPicParams);
typedef CUresult CUDAAPI tcuvidReconfigureDecoder(CUvideodecoder hDecoder, CUVIDRECONFIGUREDECODERINFO* pDecReconfigParams);
typedef CUresult CUDAAPI tcuvidMapVideoFrame64(CUvideodecoder hDecoder, int nPicIdx, unsigned long long* pDevPtr, unsigned int* pPitch, CUVIDPROCPARAMS* pVPP);
typedef CUresult CUDAAPI tcuvidUnmapVideoFrame64(CUvideodecoder hDecoder, unsigned long long DevPtr);

typedef CUresult CUDAAPI tcuGraphicsD3D11RegisterResource(CUgraphicsResource* pCudaResource, ID3D11Resource* pD3DResource, unsigned int Flags);
typedef CUresult CUDAAPI tcuGraphicsMapResources(unsigned int count, CUgraphicsResource* resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsSubResourceGetMappedArray(CUarray* pArray, CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel);
typedef CUresult CUDAAPI tcuGraphicsUnmapResources(unsigned int count, CUgraphicsResource* resources, CUstream hStream);

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define GET_PROC(name, dll)                     \
    name = reinterpret_cast<t##name *>(GetProcAddress(dll, #name));  \
    if (!name) {                           \
        OutputDebugString("Get Proc Failed");                  \
        CResult = CUDA_ERROR_UNKNOWN;                          \
        return CUDA_ERROR_UNKNOWN;                             \
    }                                                          \

#endif



class NVDecoder {
public:
    NVDecoder(UINT Width, UINT Height, ID3D11Texture2D* OutputTexture);

    CUresult InitializeNVDEC();
    void NVDecode(const unsigned char* data, unsigned long size);
    void Cleanup();

    //void RegisterResource(void* OutputTexture);
    void CompatibilityCheck();

    tcuvidCreateVideoParser* cuvidCreateVideoParser = nullptr;
    tcuvidParseVideoData* cuvidParseVideoData = nullptr;
    tcuvidDestroyVideoParser* cuvidDestroyVideoParser = nullptr;

    tcuvidGetDecoderCaps* cuvidGetDecoderCaps = nullptr;
    tcuvidCreateDecoder* cuvidCreateDecoder = nullptr;
    tcuvidDestroyDecoder* cuvidDestroyDecoder = nullptr;
    tcuvidDecodePicture* cuvidDecodePicture = nullptr;
    tcuvidReconfigureDecoder* cuvidReconfigureDecoder = nullptr;
    tcuvidMapVideoFrame64* cuvidMapVideoFrame64 = nullptr;
    tcuvidUnmapVideoFrame64* cuvidUnmapVideoFrame64 = nullptr;

    tcuGraphicsD3D11RegisterResource* cuGraphicsD3D11RegisterResource = nullptr;
    tcuGraphicsMapResources* cuGraphicsMapResources = nullptr;
    tcuGraphicsSubResourceGetMappedArray* cuGraphicsSubResourceGetMappedArray = nullptr;
    tcuGraphicsUnmapResources* cuGraphicsUnmapResources = nullptr;

private:
    CUresult CResult;
    HMODULE NvDecAPIHandle;
    HMODULE NvCudaAPIHandle;

    UINT width;
    UINT height;
    ID3D11Texture2D* OutputBuffer;
    CUgraphicsResource CudaOutputResource;


    CUcontext CudaContext;
    CUdevice CudaDevice;

    CUvideoparser CudaParser;
    CUVIDPARSERPARAMS CudaParserParams = {};

    CUvideodecoder CudaDecoder;
    CUVIDDECODECREATEINFO CudaDecoderInfo = {};





    static int CUDAAPI ParserSequenceCallback(void* instanceData, CUVIDEOFORMAT* CuDecoderInfo);

    static int CUDAAPI PictureDecodeCallback(void* instanceData, CUVIDPICPARAMS* pPicParams);

    static int CUDAAPI PictureOutputCallback(void* instanceData, CUVIDPARSERDISPINFO* pDispInfo);


    

    
};


#endif