#ifndef NVDECODER_H
#define NVDECODER_H

#pragma once
#include <string>
#include <windows.h>

#include <cuda.h>
#include <cuviddec.h>
#include <nvcuvid.h>

#pragma comment(lib, "nvcuvid.lib")
#pragma comment(lib, "cuda.lib")


typedef CUresult CUDAAPI tcuvidCreateVideoParser(CUvideoparser* pObj, CUVIDPARSERPARAMS* pParams);
typedef CUresult CUDAAPI tcuvidParseVideoData(CUvideoparser obj, CUVIDSOURCEDATAPACKET* pPacket);
typedef CUresult CUDAAPI tcuvidDestroyVideoParser(CUvideoparser obj);

typedef CUresult CUDAAPI tcuvidGetDecoderCaps(CUVIDDECODECAPS* pdc);
typedef CUresult CUDAAPI tcuvidCreateDecoder(CUvideodecoder* phDecoder, CUVIDDECODECREATEINFO* pdci);
typedef CUresult CUDAAPI tcuvidDestroyDecoder(CUvideodecoder hDecoder);
typedef CUresult CUDAAPI tcuvidDecodePicture(CUvideodecoder hDecoder, CUVIDPICPARAMS* pPicParams);
typedef CUresult CUDAAPI tcuvidReconfigureDecoder(CUvideodecoder hDecoder, CUVIDRECONFIGUREDECODERINFO* pDecReconfigParams);
typedef CUresult CUDAAPI tcuvidMapVideoFrame64(CUvideodecoder hDecoder, int nPicIdx, unsigned long long* pDevPtr, unsigned int* pPitch, CUVIDPROCPARAMS* pVPP);


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#define GET_PROC_EX(name, alias, required)                     \
    alias = reinterpret_cast<t##name *>(GetProcAddress(NvDecAPIHandle, #name));  \
    if (!alias) {                           \
        OutputDebugString("Get Proc Failed");                  \
        CResult = CUDA_ERROR_UNKNOWN;                          \
        return CUDA_ERROR_UNKNOWN;                             \
    }                                                          \

#endif

#define GET_PROC_REQUIRED(name) GET_PROC_EX(name,name,1)
#define GET_PROC_OPTIONAL(name) GET_PROC_EX(name,name,0)
#define GET_PROC(name)          GET_PROC_REQUIRED(name)


class NVDecoder {
public:
    NVDecoder(UINT Width, UINT Height);

    CUresult InitializeNVDEC();
    void NVDecode(const unsigned char* data, unsigned long size);
    void Cleanup();

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

private:
    CUresult CResult;
    HMODULE NvDecAPIHandle;

    UINT width;
    UINT height;

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