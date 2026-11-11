#include "nvdec.hpp"
#include "OmniLogger.hpp"
#include "cuviddec.h"

// Define static functions of NVDriver
tcuvidCreateVideoParser*  NVDriver::cuvidCreateVideoParser  = nullptr;
tcuvidParseVideoData*     NVDriver::cuvidParseVideoData     = nullptr;
tcuvidDestroyVideoParser* NVDriver::cuvidDestroyVideoParser = nullptr;
tcuvidGetDecoderCaps*     NVDriver::cuvidGetDecoderCaps     = nullptr;
tcuvidCreateDecoder*      NVDriver::cuvidCreateDecoder      = nullptr;
tcuvidDestroyDecoder*     NVDriver::cuvidDestroyDecoder     = nullptr;
tcuvidDecodePicture*      NVDriver::cuvidDecodePicture      = nullptr;
tcuvidReconfigureDecoder* NVDriver::cuvidReconfigureDecoder = nullptr;
tcuvidMapVideoFrame64*    NVDriver::cuvidMapVideoFrame64    = nullptr;
tcuvidUnmapVideoFrame64*  NVDriver::cuvidUnmapVideoFrame64  = nullptr;

tcuGraphicsD3D11RegisterResource*     NVDriver::cuGraphicsD3D11RegisterResource     = nullptr;
tcuGraphicsMapResources*              NVDriver::cuGraphicsMapResources              = nullptr;
tcuGraphicsSubResourceGetMappedArray* NVDriver::cuGraphicsSubResourceGetMappedArray = nullptr;
tcuGraphicsUnmapResources*            NVDriver::cuGraphicsUnmapResources            = nullptr;
tcuGraphicsUnregisterResource*        NVDriver::cuGraphicsUnregisterResource        = nullptr;

// Dynamically loading up the nvidia libraries
// Nvidia CUDA Video Decode API library and the CUDA Driver API library
// Loads up functions from API handles, Initializes CUDA Driver API
// Grabs the Compute Device and passes it on to create a new CUDA context for it
CUresult NVDriver::Initialize()
{
    if (Initialized) {
        return CResult;
    }

    NvDecAPIHandle = LoadLibrary("nvcuvid.dll");
    if (!NvDecAPIHandle) {
        Logger::log("LoadLibrary nvcuvid.dll Died!!");
        CResult = CUDA_ERROR_UNKNOWN;
        return CResult;
    }

    NvCudaAPIHandle = LoadLibrary("nvcuda.dll");
    if (!NvCudaAPIHandle) {
        Logger::log("LoadLibrary nvcuda.dll Died!!");
        CResult = CUDA_ERROR_UNKNOWN;
        return CResult;
    }

#define GET_PROC(name, dll)                                                                        \
    name = reinterpret_cast<t##name*>(GetProcAddress(dll, #name));                                 \
    if (!name) {                                                                                   \
        Logger::log("Get Proc Failed: " #name "\n");                                               \
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
        Logger::log("cuInit Failed!");
        return CResult;
    }

    CResult = cuDeviceGet(&CudaDevice, 0);
    if (CResult != CUDA_SUCCESS) {
        Logger::log("cuDeviceGet Failed!");
        return CResult;
    }

    CResult = cuCtxCreate(&CudaContext, 0, CudaDevice);
    if (CResult != CUDA_SUCCESS) {
        Logger::log("cuCtxCreate Failed!");
        return CResult;
    }

    Initialized = true;
    return CResult;
}

// If Initialized destroys CUDA conteext, frees the library API handles
void NVDriver::Release()
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

// Checking compatibility for H264/HVEC/AV1 with YUV 420/440 and 8/10 bit support
void NVDriver::CompatibilityCheck(UINT Width, UINT Height)
{
    if (!cuvidGetDecoderCaps) {
        Logger::log("cuvidGetDecoderCaps function pointer is invalid.");
        return;
    }

    // --- YUV 4:2:0 Profiles ---
    CUVIDDECODECAPS CapsH264_420 = {};
    CapsH264_420.eCodecType      = cudaVideoCodec_H264;
    CapsH264_420.eChromaFormat   = cudaVideoChromaFormat_420;
    CapsH264_420.nBitDepthMinus8 = 0;

    CUVIDDECODECAPS CapsHEVC_420_8bit = {};
    CapsHEVC_420_8bit.eCodecType      = cudaVideoCodec_HEVC;
    CapsHEVC_420_8bit.eChromaFormat   = cudaVideoChromaFormat_420;
    CapsHEVC_420_8bit.nBitDepthMinus8 = 0;

    CUVIDDECODECAPS CapsHEVC_420_10bit = {};
    CapsHEVC_420_10bit.eCodecType      = cudaVideoCodec_HEVC;
    CapsHEVC_420_10bit.eChromaFormat   = cudaVideoChromaFormat_420;
    CapsHEVC_420_10bit.nBitDepthMinus8 = 2;

    CUVIDDECODECAPS CapsAV1_420 = {};
    CapsAV1_420.eCodecType      = cudaVideoCodec_AV1;
    CapsAV1_420.eChromaFormat   = cudaVideoChromaFormat_420;
    CapsAV1_420.nBitDepthMinus8 = 0;

    // --- YUV 4:4:4 Profiles ---
    CUVIDDECODECAPS CapsH264_444 = {};
    CapsH264_444.eCodecType      = cudaVideoCodec_H264;
    CapsH264_444.eChromaFormat   = cudaVideoChromaFormat_444;
    CapsH264_444.nBitDepthMinus8 = 0;

    CUVIDDECODECAPS CapsHEVC_444_8bit = {};
    CapsHEVC_444_8bit.eCodecType      = cudaVideoCodec_HEVC;
    CapsHEVC_444_8bit.eChromaFormat   = cudaVideoChromaFormat_444;
    CapsHEVC_444_8bit.nBitDepthMinus8 = 0;

    CUVIDDECODECAPS CapsHEVC_444_10bit = {};
    CapsHEVC_444_10bit.eCodecType      = cudaVideoCodec_HEVC;
    CapsHEVC_444_10bit.eChromaFormat   = cudaVideoChromaFormat_444;
    CapsHEVC_444_10bit.nBitDepthMinus8 = 2;

    // Query Driver Capabilities
    cuvidGetDecoderCaps(&CapsH264_420);
    cuvidGetDecoderCaps(&CapsHEVC_420_8bit);
    cuvidGetDecoderCaps(&CapsHEVC_420_10bit);
    cuvidGetDecoderCaps(&CapsAV1_420);

    cuvidGetDecoderCaps(&CapsH264_444);
    cuvidGetDecoderCaps(&CapsHEVC_444_8bit);
    cuvidGetDecoderCaps(&CapsHEVC_444_10bit);

    auto ValidateCaps = [Width, Height](
                            const CUVIDDECODECAPS& Caps,
                            std::string_view       CodecName,
                            cudaVideoSurfaceFormat ExpectedOutput
                        ) {
        if (!Caps.bIsSupported) {
            Logger::log(CodecName.data());
            Logger::log("...not supported on this GPU");
            return;
        }

        if ((Width < Caps.nMinWidth) || (Width > Caps.nMaxWidth) || (Height < Caps.nMinHeight) ||
            (Height > Caps.nMaxHeight)) {
            Logger::log("Target resolution outside hardware min/max bounds.");
        }

        UINT TargetMBCount = (Width >> 4) * (Height >> 4);
        if (TargetMBCount > Caps.nMaxMBCount) {
            Logger::log("MBCount demands exceed hardware threshold.");
        }

        if (!(Caps.nOutputFormatMask & (1 << ExpectedOutput))) {
            Logger::log("Hardware missing required output surface format support.");
        }
    };

    // 4:2:0 Validations
    ValidateCaps(CapsH264_420, "H.264 (4:2:0 8-bit)", cudaVideoSurfaceFormat_NV12);
    ValidateCaps(CapsHEVC_420_8bit, "HEVC (4:2:0 8-bit)", cudaVideoSurfaceFormat_NV12);
    ValidateCaps(CapsHEVC_420_10bit, "HEVC (4:2:0 10-bit HDR)", cudaVideoSurfaceFormat_P016);
    ValidateCaps(CapsAV1_420, "AV1 (4:2:0 8-bit)", cudaVideoSurfaceFormat_NV12);

    // 4:4:4 Validations
    ValidateCaps(CapsH264_444, "H.264 (4:4:4 8-bit)", cudaVideoSurfaceFormat_YUV444);
    ValidateCaps(CapsHEVC_444_8bit, "HEVC (4:4:4 8-bit)", cudaVideoSurfaceFormat_YUV444);
    ValidateCaps(CapsHEVC_444_10bit, "HEVC (4:4:4 10-bit)", cudaVideoSurfaceFormat_YUV444_16Bit);
}

// Initializing CUDA Driver and a Decode session and registering output buffer
NvdecSession::NvdecSession(UINT MaxWidth, UINT MaxHeight, ID3D11Texture2D* OutputTexture)
    : CudaContext(NVDriver::GetContext()), CudaDevice(NVDriver::GetDevice())
{
    Width              = MaxWidth;
    Height             = MaxHeight;
    OutputBuffer       = OutputTexture;
    CudaOutputResource = nullptr;
    CudaParser         = nullptr;
    CudaDecoder        = nullptr;

    SessionResult = NVDriver::Initialize();
    if (SessionResult != CUDA_SUCCESS) {
        Logger::log(
            ("Decoder Context Initialization Failed: " + std::to_string(SessionResult)).c_str()
        );
        return;
    }

    SessionResult = InitializeSession();
    if (SessionResult != CUDA_SUCCESS) {
        Logger::log(
            ("Decoder Session Initialization Failed: " + std::to_string(SessionResult)).c_str()
        );
        return;
    }

    cuCtxPushCurrent(NVDriver::GetContext());

    SessionResult = cuGraphicsD3D11RegisterResource(&CudaOutputResource, OutputBuffer, 0);
    if (SessionResult != CUDA_SUCCESS || CudaOutputResource == NULL) {
        Logger::log(
            ("Decoder Output Texture Registration Failed: " + std::to_string(SessionResult)).c_str()
        );
    }

    cuCtxPopCurrent(nullptr);
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

    cuCtxPushCurrent(CudaContext);

    CudaDecoderInfo.CodecType      = cudaVideoCodec_H264;
    CudaDecoderInfo.ulWidth        = Width;
    CudaDecoderInfo.ulHeight       = Height;
    CudaDecoderInfo.ulTargetWidth  = Width;
    CudaDecoderInfo.ulTargetHeight = Height;
    CudaDecoderInfo.ulMaxWidth     = CudaDecoderInfo.ulWidth;
    CudaDecoderInfo.ulMaxHeight    = CudaDecoderInfo.ulHeight;

    CudaDecoderInfo.ChromaFormat        = cudaVideoChromaFormat_420;
    CudaDecoderInfo.bitDepthMinus8      = 0;
    CudaDecoderInfo.ulIntraDecodeOnly   = 0;
    CudaDecoderInfo.ulNumDecodeSurfaces = 4;
    CudaDecoderInfo.ulNumOutputSurfaces = 1;
    CudaDecoderInfo.OutputFormat        = cudaVideoSurfaceFormat_NV12;
    CudaDecoderInfo.DeinterlaceMode     = cudaVideoDeinterlaceMode_Weave;
    CudaDecoderInfo.ulCreationFlags     = cudaVideoCreate_Default;

    CudaDecoderInfo.display_area.left   = 0;
    CudaDecoderInfo.display_area.top    = 0;
    CudaDecoderInfo.display_area.right  = CudaDecoderInfo.ulWidth;
    CudaDecoderInfo.display_area.bottom = CudaDecoderInfo.ulHeight;

    CUresult CuResult = cuvidCreateDecoder(&CudaDecoder, &CudaDecoderInfo);
    if (CuResult != CUDA_SUCCESS || CudaDecoder == NULL) {
        Logger::log(("Decoder Creation Failed: " + std::to_string(CuResult)).c_str());
        cuCtxPopCurrent(nullptr);
        return CuResult;
    }

    CudaParserParams.CodecType              = cudaVideoCodec_H264;
    CudaParserParams.ulMaxNumDecodeSurfaces = 4;
    CudaParserParams.ulMaxDisplayDelay      = 0;
    CudaParserParams.pUserData              = this;
    CudaParserParams.pfnSequenceCallback    = ParserSequenceCallback;
    CudaParserParams.pfnDecodePicture       = PictureDecodeCallback;
    CudaParserParams.pfnDisplayPicture      = PictureOutputCallback;

    CuResult = cuvidCreateVideoParser(&CudaParser, &CudaParserParams);
    if (CuResult != CUDA_SUCCESS) {
        Logger::log(("Video Parser Creation Failed: " + std::to_string(CuResult)).c_str());
        cuCtxPopCurrent(nullptr);
        return CuResult;
    }

    cuCtxPopCurrent(nullptr);
    return CuResult;
}

void NvdecSession::Decode(const unsigned char* data, unsigned long size)
{
    if (!CudaParser)
        return;

    cuCtxPushCurrent(CudaContext);

    CUVIDSOURCEDATAPACKET DataPacket = {};
    DataPacket.payload               = data;
    DataPacket.payload_size          = size;
    CUresult res                     = cuvidParseVideoData(CudaParser, &DataPacket);
    if (res != CUDA_SUCCESS) {
        Logger::log(("Parser Failed: " + std::to_string(res)).c_str());
    }

    cuCtxPopCurrent(nullptr);
}

void NvdecSession::CloseSession()
{
    cuCtxPushCurrent(CudaContext);

    if (CudaParser) {
        CUVIDSOURCEDATAPACKET EosPacket = {};
        EosPacket.flags                 = CUVID_PKT_ENDOFSTREAM;
        cuvidParseVideoData(CudaParser, &EosPacket);
    }
    if (CudaOutputResource) {
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

    cuCtxPopCurrent(nullptr);
}

int CUDAAPI NvdecSession::ParserSequenceCallback(void* InstanceData, CUVIDEOFORMAT* CuDecoderInfo)
{
    NvdecSession* DecoderSession = static_cast<NvdecSession*>(InstanceData);

    CUVIDRECONFIGUREDECODERINFO ReConfig = {};
    ReConfig.ulWidth                     = CuDecoderInfo->coded_width;
    ReConfig.ulHeight                    = CuDecoderInfo->coded_height;
    ReConfig.ulTargetWidth  = CuDecoderInfo->display_area.right - CuDecoderInfo->display_area.left;
    ReConfig.ulTargetHeight = CuDecoderInfo->display_area.bottom - CuDecoderInfo->display_area.top;
    ReConfig.ulNumDecodeSurfaces = CuDecoderInfo->min_num_decode_surfaces;

    ReConfig.display_area.top    = CuDecoderInfo->display_area.top;
    ReConfig.display_area.left   = CuDecoderInfo->display_area.left;
    ReConfig.display_area.bottom = CuDecoderInfo->display_area.bottom;
    ReConfig.display_area.right  = CuDecoderInfo->display_area.right;

    ReConfig.target_rect.top    = CuDecoderInfo->display_area.top;
    ReConfig.target_rect.left   = CuDecoderInfo->display_area.left;
    ReConfig.target_rect.bottom = CuDecoderInfo->display_area.bottom;
    ReConfig.target_rect.right  = CuDecoderInfo->display_area.right;

    if (DecoderSession->CudaDecoder == NULL) {
        Logger::log("CudaDecoder is NULL in ParserSequenceCallback!");
        return 0;
    }

    cuCtxPushCurrent(DecoderSession->CudaContext);
    DecoderSession->SessionResult =
        NVDriver::cuvidReconfigureDecoder(DecoderSession->CudaDecoder, &ReConfig);
    cuCtxPopCurrent(nullptr);

    if (DecoderSession->SessionResult != CUDA_SUCCESS) {
        Logger::log(("\nDecoder Reconfiguration Failed (ignore 1 time): " +
                     std::to_string(DecoderSession->SessionResult))
                        .c_str());
        return 1;
    }

    return 1;
}

int CUDAAPI
NvdecSession::PictureDecodeCallback(void* InstanceData, CUVIDPICPARAMS* DecoderPicParams)
{
    NvdecSession* DecoderSession = static_cast<NvdecSession*>(InstanceData);

    cuCtxPushCurrent(DecoderSession->CudaContext);
    DecoderSession->SessionResult =
        NVDriver::cuvidDecodePicture(DecoderSession->CudaDecoder, DecoderPicParams);
    cuCtxPopCurrent(nullptr);

    if (DecoderSession->SessionResult != CUDA_SUCCESS) {
        Logger::log(("\nDecoder Failed: " + std::to_string(DecoderSession->SessionResult)).c_str());
        return 0;
    }
    return 1;
}

int CUDAAPI NvdecSession::PictureOutputCallback(void* InstanceData, CUVIDPARSERDISPINFO* pDispInfo)
{
    NvdecSession* DecoderSession = static_cast<NvdecSession*>(InstanceData);

    CUVIDPROCPARAMS procParams   = {};
    procParams.progressive_frame = 1;
    procParams.top_field_first   = 0;
    procParams.unpaired_field    = 0;

    cuCtxPushCurrent(DecoderSession->CudaContext);

    CUdeviceptr  dpSrcFrame       = 0;
    unsigned int pitch            = 0;
    DecoderSession->SessionResult = NVDriver::cuvidMapVideoFrame64(
        DecoderSession->CudaDecoder, pDispInfo->picture_index, &dpSrcFrame, &pitch, &procParams
    );
    if (DecoderSession->SessionResult != CUDA_SUCCESS) {
        Logger::log(("\nDecoder Output Callback Failed: " +
                     std::to_string(DecoderSession->SessionResult))
                        .c_str());
        return 0;
    }

    DecoderSession->SessionResult =
        NVDriver::cuGraphicsMapResources(1, &DecoderSession->CudaOutputResource, 0);
    if (DecoderSession->SessionResult != CUDA_SUCCESS ||
        DecoderSession->CudaOutputResource == nullptr) {
        Logger::log(("\nTexture Resource Mapping Failed: " +
                     std::to_string(DecoderSession->SessionResult))
                        .c_str());
        return 0;
    }

    CUarray MappedResArray;
    DecoderSession->SessionResult = NVDriver::cuGraphicsSubResourceGetMappedArray(
        &MappedResArray, DecoderSession->CudaOutputResource, 0, 0
    );
    if (DecoderSession->SessionResult != CUDA_SUCCESS) {
        Logger::log(("\nTexture Resource Mapped Array Retrieval Failed: " +
                     std::to_string(DecoderSession->SessionResult))
                        .c_str());
        return 0;
    }

    cudaSurfaceObject_t OutputSurface;
    cudaResourceDesc    OutputResDesc = {};
    OutputResDesc.resType             = cudaResourceTypeArray;
    OutputResDesc.res.array.array     = reinterpret_cast<cudaArray*>(MappedResArray);

    cudaError CudaResult = cudaCreateSurfaceObject(&OutputSurface, &OutputResDesc);
    if (CudaResult != cudaSuccess) {
        Logger::log(
            ("\nTexture Surface Object Creation Failed: " + std::to_string(CudaResult)).c_str()
        );
    }

    const uint8_t* NV12Buffer = reinterpret_cast<const uint8_t*>(dpSrcFrame);
    Cast2BGRA(NV12Buffer, DecoderSession->Width, DecoderSession->Height, OutputSurface, pitch);

    cudaDestroySurfaceObject(OutputSurface);

    CUDA_ARRAY_DESCRIPTOR arrayDesc = {};
    cuArrayGetDescriptor_v2(&arrayDesc, MappedResArray);

    DecoderSession->SessionResult =
        NVDriver::cuGraphicsUnmapResources(1, &DecoderSession->CudaOutputResource, 0);
    if (DecoderSession->SessionResult != CUDA_SUCCESS) {
        Logger::log(("\nTexture Resource Unmapping Failed: " +
                     std::to_string(DecoderSession->SessionResult))
                        .c_str());
        cuCtxPopCurrent(nullptr);
        return 0;
    }

    DecoderSession->SessionResult =
        NVDriver::cuvidUnmapVideoFrame64(DecoderSession->CudaDecoder, dpSrcFrame);
    if (DecoderSession->SessionResult != CUDA_SUCCESS) {
        Logger::log(("\nDecoder Output Unmap Failed: " +
                     std::to_string(DecoderSession->SessionResult))
                        .c_str());
    }

    cuCtxPopCurrent(nullptr);
    return 1;
}
