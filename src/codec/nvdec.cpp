#include "nvdec.h"

// Define static members of NVDecoder
HMODULE NVDecoder::NvDecAPIHandle = nullptr;
HMODULE NVDecoder::NvCudaAPIHandle = nullptr;
CUcontext NVDecoder::CudaContext = nullptr;
CUdevice NVDecoder::CudaDevice = 0;
bool NVDecoder::Initialized = false;
CUresult NVDecoder::CResult = CUDA_SUCCESS;

tcuvidCreateVideoParser* NVDecoder::cuvidCreateVideoParser = nullptr;
tcuvidParseVideoData* NVDecoder::cuvidParseVideoData = nullptr;
tcuvidDestroyVideoParser* NVDecoder::cuvidDestroyVideoParser = nullptr;
tcuvidGetDecoderCaps* NVDecoder::cuvidGetDecoderCaps = nullptr;
tcuvidCreateDecoder* NVDecoder::cuvidCreateDecoder = nullptr;
tcuvidDestroyDecoder* NVDecoder::cuvidDestroyDecoder = nullptr;
tcuvidDecodePicture* NVDecoder::cuvidDecodePicture = nullptr;
tcuvidReconfigureDecoder* NVDecoder::cuvidReconfigureDecoder = nullptr;
tcuvidMapVideoFrame64* NVDecoder::cuvidMapVideoFrame64 = nullptr;
tcuvidUnmapVideoFrame64* NVDecoder::cuvidUnmapVideoFrame64 = nullptr;

tcuGraphicsD3D11RegisterResource* NVDecoder::cuGraphicsD3D11RegisterResource = nullptr;
tcuGraphicsMapResources* NVDecoder::cuGraphicsMapResources = nullptr;
tcuGraphicsSubResourceGetMappedArray* NVDecoder::cuGraphicsSubResourceGetMappedArray = nullptr;
tcuGraphicsUnmapResources* NVDecoder::cuGraphicsUnmapResources = nullptr;
tcuGraphicsUnregisterResource* NVDecoder::cuGraphicsUnregisterResource = nullptr;

CUresult NVDecoder::Initialize()
{
    if (Initialized) {
        return CResult;
    }

    NvDecAPIHandle = LoadLibrary("nvcuvid.dll");
    if (!NvDecAPIHandle) {
        OutputDebugString("\n LoadLibrary nvcuvid.dll Died!!\n");
        CResult = CUDA_ERROR_UNKNOWN;
        return CResult;
    }

    NvCudaAPIHandle = LoadLibrary("nvcuda.dll");
    if (!NvCudaAPIHandle) {
        OutputDebugString("\n LoadLibrary nvcuda.dll Died!!\n");
        CResult = CUDA_ERROR_UNKNOWN;
        return CResult;
    }

#define GET_PROC(name, dll)                                                                        \
    name = reinterpret_cast<t##name*>(GetProcAddress(dll, #name));                                 \
    if (!name) {                                                                                   \
        OutputDebugString("Get Proc Failed: " #name "\n");                                         \
        CResult = CUDA_ERROR_UNKNOWN;                                                              \
        return CUDA_ERROR_UNKNOWN;                                                                 \
    }

    GET_PROC(cuvidCreateVideoParser, NvDecAPIHandle);
    GET_PROC(cuvidParseVideoData, NvDecAPIHandle);
    GET_PROC(cuvidDestroyVideoParser, NvDecAPIHandle);

    GET_PROC(cuvidGetDecoderCaps, NvDecAPIHandle);
    GET_PROC(cuvidCreateDecoder, NvDecAPIHandle);
    GET_PROC(cuvidDestroyDecoder, NvDecAPIHandle);
    GET_PROC(cuvidDecodePicture, NvDecAPIHandle);
    GET_PROC(cuvidReconfigureDecoder, NvDecAPIHandle);
    GET_PROC(cuvidMapVideoFrame64, NvDecAPIHandle);
    GET_PROC(cuvidUnmapVideoFrame64, NvDecAPIHandle);

    GET_PROC(cuGraphicsD3D11RegisterResource, NvCudaAPIHandle);
    GET_PROC(cuGraphicsMapResources, NvCudaAPIHandle);
    GET_PROC(cuGraphicsSubResourceGetMappedArray, NvCudaAPIHandle);
    GET_PROC(cuGraphicsUnmapResources, NvCudaAPIHandle);
    GET_PROC(cuGraphicsUnregisterResource, NvCudaAPIHandle);

#undef GET_PROC

    CResult = cuInit(0);
    if (CResult != CUDA_SUCCESS) {
        OutputDebugString("\ncuInit Failed!\n");
        return CResult;
    }

    CResult = cuDeviceGet(&CudaDevice, 0);
    if (CResult != CUDA_SUCCESS) {
        OutputDebugString("\ncuDeviceGet Failed!\n");
        return CResult;
    }

    CResult = cuCtxCreate(&CudaContext, 0, CudaDevice);
    if (CResult != CUDA_SUCCESS) {
        OutputDebugString("\ncuCtxCreate Failed!\n");
        return CResult;
    }

    Initialized = true;
    return CResult;
}

void NVDecoder::Release()
{
    if (Initialized) {
        if (CudaContext) {
            cuCtxDestroy(CudaContext);
            CudaContext = nullptr;
        }
        if (NvDecAPIHandle) {
            FreeLibrary(NvDecAPIHandle);
            NvDecAPIHandle = nullptr;
        }
        if (NvCudaAPIHandle) {
            FreeLibrary(NvCudaAPIHandle);
            NvCudaAPIHandle = nullptr;
        }
        Initialized = false;
    }
}

void NVDecoder::CompatibilityCheck()
{
    CUVIDDECODECAPS CudaCaps = {};
    CudaCaps.eCodecType = cudaVideoCodec_H264;
    CudaCaps.eChromaFormat = cudaVideoChromaFormat_420;
    CudaCaps.nBitDepthMinus8 = 0;

    if (cuvidGetDecoderCaps) {
        cuvidGetDecoderCaps(&CudaCaps);
    }
}

NvdecSession::NvdecSession(UINT MaxWidth, UINT MaxHeight, ID3D11Texture2D* OutputTexture)
{
    Width = MaxWidth;
    Height = MaxHeight;
    OutputBuffer = OutputTexture;
    CudaOutputResource = nullptr;
    CudaParser = nullptr;
    CudaDecoder = nullptr;

    SessionResult = Initialize();
    if (SessionResult != CUDA_SUCCESS) {
        OutputDebugString(
            ("\nDecoder Context Initialization Failed: " + std::to_string(SessionResult)).c_str()
        );
        return;
    }

    SessionResult = InitializeSession();
    if (SessionResult != CUDA_SUCCESS) {
        OutputDebugString(
            ("\nDecoder Session Initialization Failed: " + std::to_string(SessionResult)).c_str()
        );
        return;
    }

    cuCtxPushCurrent(CudaContext);

    SessionResult = cuGraphicsD3D11RegisterResource(&CudaOutputResource, OutputBuffer, 0);
    if (SessionResult != CUDA_SUCCESS || CudaOutputResource == NULL) {
        OutputDebugString(("\nDecoder Output Texture Registration Failed: " +
                           std::to_string(SessionResult))
                              .c_str());
    }
}

NvdecSession::~NvdecSession()
{
    CloseSession();
}

CUresult NvdecSession::InitializeSession()
{
    if (CudaDecoder != nullptr && CudaParser != nullptr) {
        return CUDA_SUCCESS;
    }
    CudaDecoderInfo.CodecType = cudaVideoCodec_H264;
    CudaDecoderInfo.ulWidth = Width;
    CudaDecoderInfo.ulHeight = Height;
    CudaDecoderInfo.ulTargetWidth = Width;
    CudaDecoderInfo.ulTargetHeight = Height;
    CudaDecoderInfo.ulMaxWidth = CudaDecoderInfo.ulWidth;
    CudaDecoderInfo.ulMaxHeight = CudaDecoderInfo.ulHeight;

    CudaDecoderInfo.ChromaFormat = cudaVideoChromaFormat_420;
    CudaDecoderInfo.bitDepthMinus8 = 0;
    CudaDecoderInfo.ulIntraDecodeOnly = 1;
    CudaDecoderInfo.ulNumDecodeSurfaces = 8;
    CudaDecoderInfo.ulNumOutputSurfaces = 2;
    CudaDecoderInfo.OutputFormat = cudaVideoSurfaceFormat_NV12;
    CudaDecoderInfo.DeinterlaceMode = cudaVideoDeinterlaceMode_Weave;
    CudaDecoderInfo.ulCreationFlags = cudaVideoCreate_Default;

    CudaDecoderInfo.display_area.left = 0;
    CudaDecoderInfo.display_area.top = 0;
    CudaDecoderInfo.display_area.right = CudaDecoderInfo.ulWidth;
    CudaDecoderInfo.display_area.bottom = CudaDecoderInfo.ulHeight;

    CUresult res = cuvidCreateDecoder(&CudaDecoder, &CudaDecoderInfo);
    if (res != CUDA_SUCCESS || CudaDecoder == NULL) {
        OutputDebugString(("\nDecoder Creation Failed: " + std::to_string(res)).c_str());
        return res;
    }

    CudaParserParams.CodecType = cudaVideoCodec_H264;
    CudaParserParams.ulMaxNumDecodeSurfaces = 8;
    CudaParserParams.ulMaxDisplayDelay = 0;
    CudaParserParams.pUserData = this;
    CudaParserParams.pfnSequenceCallback = ParserSequenceCallback;
    CudaParserParams.pfnDecodePicture = PictureDecodeCallback;
    CudaParserParams.pfnDisplayPicture = PictureOutputCallback;

    res = cuvidCreateVideoParser(&CudaParser, &CudaParserParams);
    if (res != CUDA_SUCCESS) {
        OutputDebugString(("\nVideo Parser Creation Failed: " + std::to_string(res)).c_str());
        return res;
    }

    return res;
}

void NvdecSession::Decode(const unsigned char* data, unsigned long size)
{
    if (SessionResult != CUDA_SUCCESS || !CudaParser)
        return;

    CUVIDSOURCEDATAPACKET DataPacket = {};
    DataPacket.payload = data;
    DataPacket.payload_size = size;
    CUresult res = cuvidParseVideoData(CudaParser, &DataPacket);
    if (res != CUDA_SUCCESS) {
        OutputDebugString(("\nParser Failed: " + std::to_string(res)).c_str());
    }
}

void NvdecSession::CloseSession()
{
    if (CudaParser) {
        CUVIDSOURCEDATAPACKET EosPacket = {};
        EosPacket.flags = CUVID_PKT_ENDOFSTREAM;
        cuvidParseVideoData(CudaParser, &EosPacket);
    }
    if (CudaOutputResource) {
        cuCtxPushCurrent(CudaContext);
        cuGraphicsUnregisterResource(CudaOutputResource);
        CudaOutputResource = nullptr;
    }
    if (CudaParser) {
        cuvidDestroyVideoParser(CudaParser);
        CudaParser = nullptr;
    }
    if (CudaDecoder) {
        cuvidDestroyDecoder(CudaDecoder);
        CudaDecoder = nullptr;
    }
}

int CUDAAPI NvdecSession::ParserSequenceCallback(void* instanceData, CUVIDEOFORMAT* CuDecoderInfo)
{
    NvdecSession* instance = static_cast<NvdecSession*>(instanceData);

    CUVIDRECONFIGUREDECODERINFO ReConfig = {};

    /*ReConfig.ulWidth = CuDecoderInfo->coded_width;
    ReConfig.ulHeight = CuDecoderInfo->coded_height;
    ReConfig.ulTargetWidth = CuDecoderInfo->display_area.right - CuDecoderInfo->display_area.left;
    ReConfig.ulTargetHeight = CuDecoderInfo->display_area.bottom - CuDecoderInfo->display_area.top;
    ReConfig.ulNumDecodeSurfaces = CuDecoderInfo->min_num_decode_surfaces;

    ReConfig.display_area.top = CuDecoderInfo->display_area.top;
    ReConfig.display_area.left = CuDecoderInfo->display_area.left;
    ReConfig.display_area.bottom = CuDecoderInfo->display_area.bottom;
    ReConfig.display_area.right = CuDecoderInfo->display_area.right;

    ReConfig.target_rect.top = CuDecoderInfo->display_area.top;
    ReConfig.target_rect.left = CuDecoderInfo->display_area.left;
    ReConfig.target_rect.bottom = CuDecoderInfo->display_area.bottom;
    ReConfig.target_rect.right = CuDecoderInfo->display_area.right;*/

    if (instance->CudaDecoder == NULL) {
        OutputDebugString("CudaDecoder is NULL in ParserSequenceCallback!");
    }

    instance->SessionResult = instance->cuvidReconfigureDecoder(instance->CudaDecoder, &ReConfig);
    if (instance->SessionResult != CUDA_SUCCESS) {
        OutputDebugString(("\nDecoder Reconfiguration Failed (ignore 1 time): " +
                           std::to_string(instance->SessionResult))
                              .c_str());
        return 1;
    }

    return 1;
}

int CUDAAPI
NvdecSession::PictureDecodeCallback(void* instanceData, CUVIDPICPARAMS* DecoderPicParams)
{
    NvdecSession* instance = static_cast<NvdecSession*>(instanceData);

    instance->SessionResult = instance->cuvidDecodePicture(instance->CudaDecoder, DecoderPicParams);
    if (instance->SessionResult != CUDA_SUCCESS) {
        OutputDebugString(("\nDecoder Failed: " + std::to_string(instance->SessionResult)).c_str());
        return 0;
    }
    return 1;
}

int CUDAAPI NvdecSession::PictureOutputCallback(void* instanceData, CUVIDPARSERDISPINFO* pDispInfo)
{
    NvdecSession* instance = static_cast<NvdecSession*>(instanceData);

    CUVIDPROCPARAMS procParams = {};
    procParams.progressive_frame = 1;
    procParams.top_field_first = 0;
    procParams.unpaired_field = 0;

    cuCtxPushCurrent(instance->CudaContext);

    CUdeviceptr dpSrcFrame = 0;
    unsigned int pitch = 0;
    instance->SessionResult = instance->cuvidMapVideoFrame(
        instance->CudaDecoder, pDispInfo->picture_index, &dpSrcFrame, &pitch, &procParams
    );
    if (instance->SessionResult != CUDA_SUCCESS) {
        OutputDebugString(
            ("\nDecoder Output Callback Failed: " + std::to_string(instance->SessionResult)).c_str()
        );
        return 0;
    }

    instance->SessionResult = instance->cuGraphicsMapResources(1, &instance->CudaOutputResource, 0);
    if (instance->SessionResult != CUDA_SUCCESS || instance->CudaOutputResource == nullptr) {
        OutputDebugString(("\nTexture Resource Mapping Failed: " +
                           std::to_string(instance->SessionResult))
                              .c_str());
        return 0;
    }

    CUarray MappedResArray;
    instance->SessionResult = instance->cuGraphicsSubResourceGetMappedArray(
        &MappedResArray, instance->CudaOutputResource, 0, 0
    );
    if (instance->SessionResult != CUDA_SUCCESS) {
        OutputDebugString(("\nTexture Resource Mapped Array Retrieval Failed: " +
                           std::to_string(instance->SessionResult))
                              .c_str());
        return 0;
    }

    cudaSurfaceObject_t OutputSurface;
    cudaResourceDesc OutputResDesc = {};
    OutputResDesc.resType = cudaResourceTypeArray;
    OutputResDesc.res.array.array = reinterpret_cast<cudaArray*>(MappedResArray);

    cudaError CudaResult = cudaCreateSurfaceObject(&OutputSurface, &OutputResDesc);
    if (CudaResult != cudaSuccess) {
        OutputDebugString(
            ("\nTexture Surface Object Creation Failed: " + std::to_string(CudaResult)).c_str()
        );
    }

    const uint8_t* NV12Buffer = reinterpret_cast<const uint8_t*>(dpSrcFrame);
    Cast2BGRA(NV12Buffer, instance->Width, instance->Height, OutputSurface, pitch);

    CUDA_ARRAY_DESCRIPTOR arrayDesc = {};
    cuArrayGetDescriptor_v2(&arrayDesc, MappedResArray);

    instance->SessionResult =
        instance->cuGraphicsUnmapResources(1, &instance->CudaOutputResource, 0);
    if (instance->SessionResult != CUDA_SUCCESS) {
        OutputDebugString(("\nTexture Resource Unmapping Failed: " +
                           std::to_string(instance->SessionResult))
                              .c_str());
        return 0;
    }

    instance->SessionResult = instance->cuvidUnmapVideoFrame(instance->CudaDecoder, dpSrcFrame);
    if (instance->SessionResult != CUDA_SUCCESS) {
        OutputDebugString(
            ("\nDecoder Output Unmap Failed: " + std::to_string(instance->SessionResult)).c_str()
        );
    }
    return 1;
}
