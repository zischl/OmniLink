#include "nvenc.h"
#include "OmniLogger.h"

#define NVCHECK(status, error)                                                                     \
    {                                                                                              \
        if (status != NV_ENC_SUCCESS) {                                                            \
            std::string text = error;                                                              \
            Logger::log((text + " : " + std::to_string(status)).c_str());                          \
        }                                                                                          \
    }

NVENCODER::NVENCODER()
{
    LoadNvEncodeAPI();
}

NVENCODER::~NVENCODER() {}

void NVENCODER::LoadNvEncodeAPI()
{
    HMODULE API_Handle = LoadLibrary("nvencodeapi64.dll");
    if (!API_Handle) {
        Logger::log("\n LoadLibrary Died!!");
        return;
    }

    typedef NVENCSTATUS(NVENCAPI * PFN_NvEncodeAPICreateInstance)(NV_ENCODE_API_FUNCTION_LIST*);

    auto NvEncodeAPICreateInstance =
        (PFN_NvEncodeAPICreateInstance)GetProcAddress(API_Handle, "NvEncodeAPICreateInstance");
    if (!NvEncodeAPICreateInstance) {
        Logger::log("Encode API Instance Creation Failed -_- \n");
        return;
    }

    NVFunctions.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    Status = NvEncodeAPICreateInstance(&NVFunctions);
    if (Status != NV_ENC_SUCCESS) {
        Logger::log("Encode API Function Filling Failed -_- \n");
    }
}

void NVENCODER::GetSupportedCodecGUIDs(void* NVEncoder, NV_ENCODE_API_FUNCTION_LIST& NVFunctions_)
{
    NVENCSTATUS status;

    uint32_t NvencGUIDCount;
    NVFunctions_.nvEncGetEncodeGUIDCount(NVEncoder, &NvencGUIDCount);
    std::vector<GUID> NvencGUIDs(NvencGUIDCount);
    status = NVFunctions_.nvEncGetEncodeGUIDs(
        NVEncoder, NvencGUIDs.data(), NvencGUIDCount, &NvencGUIDCount
    );
    if (status != NV_ENC_SUCCESS) {
        Logger::log("RIP Encode GUIDS \n");
    }
    for (GUID guid : NvencGUIDs) {
        Logger::log(("Supported format: " + std::to_string(guid.Data1) + "\n").c_str());
        Logger::log(("Supported format: " + std::to_string(guid.Data2) + "\n").c_str());
        Logger::log(("Supported format: " + std::to_string(guid.Data3) + "\n").c_str());
        Logger::log(("Supported format: " + std::to_string(guid.Data4[0]) + "\n").c_str());
        Logger::log(("Supported format: " + std::to_string(guid.Data4[1]) + "\n").c_str());
        Logger::log(("Supported format: " + std::to_string(guid.Data4[2]) + "\n").c_str());
    }
}

void NVENCODER::GetAvailablePresetGUIDs(void* NVEncoder)
{
    /*uint32_t NvencPresetCount;
    NVFunctions.nvEncGetEncodePresetCount(NVEncoder, , &NvencPresetCount);
    GUID NVPresetGUIDs;
    status = NVFunctions.nvEncGetEncodePresetGUIDs(NVEncoder, NvencEncodeGUID, &NVPresetGUIDs,
    NvencPresetCount, &NvencPresetCount); if (status != NV_ENC_SUCCESS) { Logger::log("RIP Encode
    Preset GUIDS \n");
    }*/
}

void NVENCODER::GetAvailableProfileGUIDs(void* NVEncoder, GUID NvencCodecGUID)
{

    NVENCSTATUS Status;

    uint32_t NvenvProfileGUIDCount;
    GUID NvProfileGUIDs;
    NVFunctions.nvEncGetEncodeProfileGUIDCount(NVEncoder, NvencCodecGUID, &NvenvProfileGUIDCount);
    Status = NVFunctions.nvEncGetEncodeProfileGUIDs(
        NVEncoder, NvencCodecGUID, &NvProfileGUIDs, NvenvProfileGUIDCount, &NvenvProfileGUIDCount
    );
    if (Status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Profile GUID \n" + std::to_string(Status)).c_str());
    }
}

void NVENCODER::GetSupportedInputFormats(void* NVEncoder, GUID NvencCodecGUID)
{

    NVENCSTATUS Status;

    uint32_t NvenvInputFormatCount = 0;
    Status =
        NVFunctions.nvEncGetInputFormatCount(NVEncoder, NvencCodecGUID, &NvenvInputFormatCount);
    if (Status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Input Format Count \n" + std::to_string(Status)).c_str());
    }

    std::vector<NV_ENC_BUFFER_FORMAT> NvBufferFormats(NvenvInputFormatCount);
    Status = NVFunctions.nvEncGetInputFormats(
        NVEncoder,
        NvencCodecGUID,
        NvBufferFormats.data(),
        NvenvInputFormatCount,
        &NvenvInputFormatCount
    );
    if (Status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Input Formats \n" + std::to_string(Status)).c_str());
    }

    for (auto fmt : NvBufferFormats) {
        Logger::log(("Supported Input format: " + std::to_string(fmt) + "\n").c_str());
    }
}

NvencSession::NvencSession(
    void* D3DDevice, NV_ENCODE_API_FUNCTION_LIST& NVFunctions_, UINT EncodeWidth, UINT EncodeHeight
)
    : NVFunctions(NVFunctions_)
{
    OpenNvEncSession(D3DDevice);

    NV_ENC_INITIALIZE_PARAMS NvInitParams = {};
    NvencInitConfig NvInitConfig = {};
    NvInitConfig.Dimensions.Width = EncodeWidth;
    NvInitConfig.Dimensions.Height = EncodeHeight;
    LoadDefaultInitParams(NvInitParams, NvInitConfig);
    NVEncoderInit(NvInitParams);

    ResourceConfig = {NvInitConfig.Dimensions, NvInitConfig.NvencBufferFormat};
    CreateBitStream();
}

NvencSession::~NvencSession()
{
    if (NvencOutput) {
        Status = NVFunctions.nvEncDestroyBitstreamBuffer(NVEncoder, NvencOutput);
        NVCHECK(Status, "Nvenc Bit Stream Buffer Could Not Be Destroyed");
        NvencOutput = nullptr;
    }

    if (NVEncoder) {
        Status = NVFunctions.nvEncDestroyEncoder(NVEncoder);
        NVCHECK(Status, "Nv Encoder Could Not Be Destroyed");
        NVEncoder = nullptr;
    }
}

void NvencSession::OpenNvEncSession(void* D3DDevice)
{
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS NVSessionParams = {};
    NVSessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    NVSessionParams.apiVersion = NVENCAPI_VERSION;
    NVSessionParams.device = D3DDevice;
    NVSessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    Status = NVFunctions.nvEncOpenEncodeSessionEx(&NVSessionParams, &NVEncoder);

    if (Status != NV_ENC_SUCCESS || NVEncoder == nullptr) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log("Encoder did not feel like coming home. Prolly \n");
    }
}

void NvencSession::LoadDefaultInitParams(
    NV_ENC_INITIALIZE_PARAMS& NvInitParams, NvencInitConfig& config
)
{

    NVENCODER::GetSupportedCodecGUIDs(NVEncoder, NVFunctions);

    NV_ENC_PRESET_CONFIG NVPresetConfig = {};
    NVPresetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    NVPresetConfig.presetCfg.version = NV_ENC_CONFIG_VER;

    Status = NVFunctions.nvEncGetEncodePresetConfigEx(
        NVEncoder,
        config.NvencCodecGUID,
        config.NvencPresetGUID,
        config.NvencTuningInfo,
        &NVPresetConfig
    );
    if (Status != NV_ENC_SUCCESS || NVEncoder == nullptr) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log("Encoder did not feel like giving me the preset config \n");
    }

    NVPresetConfig.presetCfg.gopLength = config.gopLength;
    NVPresetConfig.presetCfg.frameIntervalP = config.frameIntervalP;

    NVPresetConfig.presetCfg.rcParams.averageBitRate = config.rcParams.averageBitRate;
    NVPresetConfig.presetCfg.rcParams.maxBitRate = config.rcParams.maxBitRate;

    // I would prolly let the user decide on the multiplier later
    if (config.rcParams.averageBitRate > 0) {
        const uint32_t fps = (config.FrameRateNum > 0) ? config.FrameRateNum : 60;
        const uint32_t onFrameBits = config.rcParams.averageBitRate / fps;
        NVPresetConfig.presetCfg.rcParams.vbvBufferSize = onFrameBits * 4;
        NVPresetConfig.presetCfg.rcParams.vbvInitialDelay = onFrameBits * 4;
    }

    NVPresetConfig.presetCfg.rcParams.enableLookahead = config.rcParams.enableLookahead;
    NVPresetConfig.presetCfg.rcParams.lookaheadDepth = config.rcParams.enableLookahead;
    NVPresetConfig.presetCfg.rcParams.disableIadapt = config.rcParams.disableIadapt;
    NVPresetConfig.presetCfg.rcParams.disableBadapt = config.rcParams.disableBadapt;

    NVPresetConfig.presetCfg.rcParams.enableAQ = config.rcParams.enableAQ;
    NVPresetConfig.presetCfg.rcParams.aqStrength = config.rcParams.aqStrength;

    auto& h264 = NVPresetConfig.presetCfg.encodeCodecConfig.h264Config;

    h264.idrPeriod = config.h264Config.idrPeriod;
    h264.repeatSPSPPS = config.h264Config.repeatSPSPPS;
    h264.disableDeblockingFilterIDC = config.h264Config.disableDeblockingFilterIDC;

    h264.enableIntraRefresh = config.h264Config.enableIntraRefresh;
    h264.intraRefreshPeriod = config.h264Config.intraRefreshPeriod;
    h264.intraRefreshCnt = config.h264Config.intraRefreshCnt;

    h264.level = config.h264Config.level;
    h264.maxNumRefFrames = config.h264Config.maxNumRefFrames;
    h264.entropyCodingMode = config.h264Config.entropyCodingMode;
    h264.sliceMode = config.h264Config.sliceMode;

    NvInitParams.encodeConfig = &NVPresetConfig.presetCfg;

    NvInitParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
    NvInitParams.bufferFormat = config.NvencBufferFormat;
    NvInitParams.encodeGUID = config.NvencCodecGUID;
    NvInitParams.presetGUID = config.NvencPresetGUID;
    NvInitParams.tuningInfo = config.NvencTuningInfo;
    NvInitParams.encodeWidth = config.Dimensions.Width;
    NvInitParams.encodeHeight = config.Dimensions.Height;
    NvInitParams.darWidth = config.Dimensions.Width;
    NvInitParams.darHeight = config.Dimensions.Height;
    NvInitParams.frameRateNum = config.FrameRateNum;
    NvInitParams.frameRateDen = config.FrameRateDen;
    NvInitParams.enablePTD = config.EnablePTD;
    NvInitParams.enableEncodeAsync = 0;
}

void NvencSession::NVEncoderInit(NV_ENC_INITIALIZE_PARAMS& NvInitParams)
{
    Status = NVFunctions.nvEncInitializeEncoder(NVEncoder, &NvInitParams);
    if (Status != NV_ENC_SUCCESS) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log((" RIP Encoder Init " + std::to_string(Status) + "\n").c_str());
    }
}

NV_ENC_REGISTERED_PTR
NvencSession::RegisterResource(ID3D11Texture2D* inputResource, const NvencResourceRegConfig& config)
{
    NV_ENC_REGISTER_RESOURCE regParam = {};
    regParam.version = NV_ENC_REGISTER_RESOURCE_VER;
    regParam.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    regParam.resourceToRegister = inputResource;
    regParam.width = config.Dimensions.Width;
    regParam.height = config.Dimensions.Height;
    regParam.bufferFormat = config.NvencBufferFormat;
    regParam.pitch = config.Dimensions.Width * 4;

    Status = NVFunctions.nvEncRegisterResource(NVEncoder, &regParam);
    if (Status != NV_ENC_SUCCESS) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log(("RIP Encoder Input Resource Marriage \n" + std::to_string(Status)).c_str());
        return nullptr;
    }
    return regParam.registeredResource;
}

void NvencSession::UnregisterResource(NV_ENC_REGISTERED_PTR registeredHandle)
{
    if (registeredHandle) {
        Status = NVFunctions.nvEncUnregisterResource(NVEncoder, registeredHandle);
        NVCHECK(Status, "Nvenc Input Resource Failed To Divorce");
    }
}

NV_ENC_REGISTERED_PTR NvencSession::SwapResource(
    NV_ENC_REGISTERED_PTR oldHandle, ID3D11Texture2D* newTex, const NvencResourceRegConfig& config
)
{
    UnregisterResource(oldHandle);
    return RegisterResource(newTex, config);
}

void NvencSession::CreateBitStream()
{

    NVOutputBufferDesc.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;

    Status = NVFunctions.nvEncCreateBitstreamBuffer(NVEncoder, &NVOutputBufferDesc);
    if (Status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Output Stream Buffer \n" + std::to_string(Status)).c_str());
    }
    NvencOutput = NVOutputBufferDesc.bitstreamBuffer;
}

bool NvencSession::Encode()
{

    NV_ENC_MAP_INPUT_RESOURCE NVInputResource = {};
    NVInputResource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    NVInputResource.registeredResource = NVRegisterResource.registeredResource;
    Status = NVFunctions.nvEncMapInputResource(NVEncoder, &NVInputResource);
    if (Status != NV_ENC_SUCCESS) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log(("RIP Input Resource Map \n" + std::to_string(Status)).c_str());
        return false;
    }

    bool ForceIDR = ForceNextIDR.exchange(false, std::memory_order_relaxed);

    NV_ENC_PIC_PARAMS NvencPicParams = {};
    memset(&NvencPicParams, 0, sizeof(NV_ENC_PIC_PARAMS));

    NvencPicParams.version = NV_ENC_PIC_PARAMS_VER;
    NvencPicParams.inputBuffer = NVInputResource.mappedResource;
    NvencPicParams.bufferFmt = NVInputResource.mappedBufferFmt;
    NvencPicParams.outputBitstream = NvencOutput;
    NvencPicParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    NvencPicParams.encodePicFlags = ForceIDR ? NV_ENC_PIC_FLAG_FORCEIDR : 0;
    NvencPicParams.completionEvent = nullptr;

    Status = NVFunctions.nvEncEncodePicture((void*)NVEncoder, &NvencPicParams);
    NVFunctions.nvEncUnmapInputResource(NVEncoder, NVInputResource.mappedResource);
    if (Status == NV_ENC_SUCCESS) {
        NVBitstreamLock = {};
        NVBitstreamLock.version = NV_ENC_LOCK_BITSTREAM_VER;
        NVBitstreamLock.outputBitstream = NvencOutput;
        NVBitstreamLock.doNotWait = 0;

        Status = NVFunctions.nvEncLockBitstream(NVEncoder, &NVBitstreamLock);
        if (Status != NV_ENC_SUCCESS) {
            Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
            Logger::log(("\n RIP Output Lock " + std::to_string(Status)).c_str());
            return false;
        }

    } else {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log(("\n RIP Encoding " + std::to_string(Status)).c_str());
        return false;
    }
    return true;
}

void NvencSession::NVUnlockBitStream()
{
    NVFunctions.nvEncUnlockBitstream(NVEncoder, NvencOutput);
}

void NvencSession::NVCleanup()
{
    if (NvencOutput) {
        Status = NVFunctions.nvEncDestroyBitstreamBuffer(NVEncoder, NvencOutput);
        NVCHECK(Status, "Nvenc Bit Stream Buffer Could Not Be Destroyed");
        NvencOutput = nullptr;
    }

    if (NVEncoder) {
        Status = NVFunctions.nvEncDestroyEncoder(NVEncoder);
        NVCHECK(Status, "Nv Encoder Could Not Be Destroyed");
        NVEncoder = nullptr;
    }
}

StaticNvencSession::StaticNvencSession(
    void* D3DDevice,
    NV_ENCODE_API_FUNCTION_LIST& NVFunctions_,
    ID3D11Texture2D* InputResource,
    UINT EncodeWidth,
    UINT EncodeHeight
)
    : NvencSession(D3DDevice, NVFunctions_, EncodeWidth, EncodeHeight)
{
    if (InputResource) {
        NVRegisterResource.registeredResource = RegisterResource(InputResource, ResourceConfig);
    }
}

StaticNvencSession::~StaticNvencSession()
{
    NVCleanup();
}

void StaticNvencSession::NVCleanup()
{
    if (NVRegisterResource.registeredResource) {
        UnregisterResource(NVRegisterResource.registeredResource);
        NVRegisterResource.registeredResource = nullptr;
    }
    NvencSession::NVCleanup();
}

CachedPoolNvencSession::CachedPoolNvencSession(
    void* D3DDevice,
    NV_ENCODE_API_FUNCTION_LIST& NVFunctions_,
    UINT EncodeWidth,
    UINT EncodeHeight,
    size_t PoolSize
)
    : NvencSession(D3DDevice, NVFunctions_, EncodeWidth, EncodeHeight)
{
    PoolCache.resize(PoolSize);
}

CachedPoolNvencSession::~CachedPoolNvencSession()
{
    NVCleanup();
}

void CachedPoolNvencSession::NVCleanup()
{
    for (auto& slot : PoolCache) {
        if (slot.NvRegisteredHandle) {
            UnregisterResource(slot.NvRegisteredHandle);
            slot.NvRegisteredHandle = nullptr;
            slot.D3DTexture.Reset();
            slot.SurfaceRawPtr = nullptr;
        }
    }
    NVRegisterResource.registeredResource = nullptr;
    NvencSession::NVCleanup();
}

void CachedPoolNvencSession::ResolveCachedResource(ID3D11Texture2D* newTex)
{
    IUnknown* TextureID = nullptr;
    newTex->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&TextureID));
    TextureID->Release();

    // Cache lookup
    for (auto& Slot : PoolCache) {
        if (Slot.SurfaceRawPtr == TextureID) {
            NVRegisterResource.registeredResource = Slot.NvRegisteredHandle;
            return;
        }
    }

    // Cache missed, get packing and register
    for (auto& Slot : PoolCache) {
        if (!Slot.SurfaceRawPtr) {
            Slot.SurfaceRawPtr = TextureID;
            Slot.D3DTexture = newTex;
            Slot.NvRegisteredHandle = RegisterResource(newTex, ResourceConfig);
            NVRegisterResource.registeredResource = Slot.NvRegisteredHandle;
            return;
        }
    }

    Logger::log("ResolveCachedResource: pool cache full — frame dropped\n");
}

static void NvencOutputTest(NV_ENC_LOCK_BITSTREAM& NVBitstreamLock, const char* baseName)
{
    static uint64_t frameIndex = 0;

    if (!NVBitstreamLock.bitstreamBufferPtr || NVBitstreamLock.bitstreamSizeInBytes == 0) {
        Logger::log("NvencOutputTest: empty bitstream\n");
        return;
    }

    char fileName[512];
    std::snprintf(
        fileName,
        sizeof(fileName),
        "%s_%llu.h264",
        baseName,
        static_cast<unsigned long long>(frameIndex++)
    );

    FILE* outFile = std::fopen(fileName, "wb");
    if (outFile) {
        std::fwrite(
            NVBitstreamLock.bitstreamBufferPtr, 1, NVBitstreamLock.bitstreamSizeInBytes, outFile
        );
        std::fclose(outFile);
    } else {
        Logger::log("Failed to open output file\n");
    }
}
