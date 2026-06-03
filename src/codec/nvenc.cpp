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
    status = NvEncodeAPICreateInstance(&NVFunctions);
    if (status != NV_ENC_SUCCESS) {
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
        NVEncoder, NvencGUIDs.data(), NvencGUIDCount, &NvencGUIDCount);
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

    NVENCSTATUS status;

    uint32_t NvenvProfileGUIDCount;
    GUID NvProfileGUIDs;
    NVFunctions.nvEncGetEncodeProfileGUIDCount(NVEncoder, NvencCodecGUID, &NvenvProfileGUIDCount);
    status = NVFunctions.nvEncGetEncodeProfileGUIDs(
        NVEncoder, NvencCodecGUID, &NvProfileGUIDs, NvenvProfileGUIDCount, &NvenvProfileGUIDCount);
    if (status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Profile GUID \n" + std::to_string(status)).c_str());
    }
}

void NVENCODER::GetSupportedInputFormats(void* NVEncoder, GUID NvencCodecGUID)
{

    NVENCSTATUS status;

    uint32_t NvenvInputFormatCount = 0;
    status =
        NVFunctions.nvEncGetInputFormatCount(NVEncoder, NvencCodecGUID, &NvenvInputFormatCount);
    if (status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Input Format Count \n" + std::to_string(status)).c_str());
    }

    std::vector<NV_ENC_BUFFER_FORMAT> NvBufferFormats(NvenvInputFormatCount);
    status = NVFunctions.nvEncGetInputFormats(NVEncoder,
                                              NvencCodecGUID,
                                              NvBufferFormats.data(),
                                              NvenvInputFormatCount,
                                              &NvenvInputFormatCount);
    if (status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Input Formats \n" + std::to_string(status)).c_str());
    }

    for (auto fmt : NvBufferFormats) {
        Logger::log(("Supported Input format: " + std::to_string(fmt) + "\n").c_str());
    }
}

NvencSession::NvencSession(void* D3DDevice,
                           NV_ENCODE_API_FUNCTION_LIST& NVFunctions_,
                           ID3D11Texture2D* inputResource,
                           UINT encodeWidth,
                           UINT encodeHeight)
    : NVFunctions(NVFunctions_)
{
    OpenNvEncSession(D3DDevice);

    NV_ENC_INITIALIZE_PARAMS NvInitParams = {};
    NvencInitConfig NvInitConfig = {};
    LoadDefaultInitParams(NvInitParams, NvInitConfig);
    NVEncoderInit(NvInitParams);

    NvencResourceRegConfig InputResourceConfig = {NvInitConfig.Dimensions,
                                                  NvInitConfig.NvencBufferFormat};
    RegisterResource(inputResource, InputResourceConfig);
    CreateBitStream();
}

void NvencSession::OpenNvEncSession(void* D3DDevice)
{
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS NVSessionParams = {};
    NVSessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    NVSessionParams.apiVersion = NVENCAPI_VERSION;
    NVSessionParams.device = D3DDevice;
    NVSessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    status = NVFunctions.nvEncOpenEncodeSessionEx(&NVSessionParams, &NVEncoder);

    if (status != NV_ENC_SUCCESS || NVEncoder == nullptr) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log("Encoder did not feel like coming home. Prolly \n");
    }
}

void NvencSession::LoadDefaultInitParams(NV_ENC_INITIALIZE_PARAMS& NvInitParams,
                                         NvencInitConfig& config)
{

    NVENCODER::GetSupportedCodecGUIDs(NVEncoder, NVFunctions);

    NV_ENC_PRESET_CONFIG NVPresetConfig = {};
    NVPresetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    NVPresetConfig.presetCfg.version = NV_ENC_CONFIG_VER;

    status = NVFunctions.nvEncGetEncodePresetConfigEx(NVEncoder,
                                                      config.NvencCodecGUID,
                                                      config.NvencPresetGUID,
                                                      config.NvencTuningInfo,
                                                      &NVPresetConfig);
    if (status != NV_ENC_SUCCESS || NVEncoder == nullptr) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log("Encoder did not feel like giving me the preset config \n");
    }

    NVPresetConfig.presetCfg.gopLength = config.gopLength;
    NVPresetConfig.presetCfg.frameIntervalP = config.frameIntervalP;

    NVPresetConfig.presetCfg.rcParams.rateControlMode = config.rcParams.rateControlMode;

    // NVPresetConfig.presetCfg.rcParams.averageBitRate = 0;
    NVPresetConfig.presetCfg.rcParams.maxBitRate = config.rcParams.averageBitRate;
    // NVPresetConfig.presetCfg.rcParams.vbvBufferSize = 0;
    // NVPresetConfig.presetCfg.rcParams.vbvInitialDelay = 0;

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
    NvInitParams.frameRateNum = config.FrameRateDen;
    NvInitParams.frameRateDen = config.FrameRateDen;
    NvInitParams.enablePTD = config.EnablePTD;
    NvInitParams.enableEncodeAsync = 0;
}

void NvencSession::NVEncoderInit(NV_ENC_INITIALIZE_PARAMS& NvInitParams)
{
    status = NVFunctions.nvEncInitializeEncoder(NVEncoder, &NvInitParams);
    if (status != NV_ENC_SUCCESS) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log((" RIP Encoder Init " + std::to_string(status) + "\n").c_str());
    }
}

void NvencSession::RegisterResource(ID3D11Texture2D* inputResource, NvencResourceRegConfig& Config)
{
    NVRegisterResource.version = NV_ENC_REGISTER_RESOURCE_VER;
    NVRegisterResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    NVRegisterResource.resourceToRegister = inputResource;
    NVRegisterResource.width = Config.Dimensions.Width;
    NVRegisterResource.height = Config.Dimensions.Height;
    NVRegisterResource.bufferFormat = Config.NvencBufferFormat;
    NVRegisterResource.pitch = Config.Dimensions.Width * 4;

    status = NVFunctions.nvEncRegisterResource(NVEncoder, &NVRegisterResource);
    if (status != NV_ENC_SUCCESS) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log(("RIP Encoder Input Resource Marriage \n" + std::to_string(status)).c_str());
    }
}

void NvencSession::CreateBitStream()
{

    NVOutputBufferDesc.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;

    status = NVFunctions.nvEncCreateBitstreamBuffer(NVEncoder, &NVOutputBufferDesc);
    if (status != NV_ENC_SUCCESS) {
        Logger::log(("RIP Encode Output Stream Buffer \n" + std::to_string(status)).c_str());
    }
    NvencOutput = NVOutputBufferDesc.bitstreamBuffer;
}

void NvencSession::Encode()
{

    NV_ENC_MAP_INPUT_RESOURCE NVInputResource = {};
    NVInputResource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    NVInputResource.registeredResource = NVRegisterResource.registeredResource;
    status = NVFunctions.nvEncMapInputResource(NVEncoder, &NVInputResource);
    if (status != NV_ENC_SUCCESS) {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log(("RIP Input Resource Map \n" + std::to_string(status)).c_str());
    }

    NV_ENC_PIC_PARAMS NvencPicParams = {};
    memset(&NvencPicParams, 0, sizeof(NV_ENC_PIC_PARAMS));

    NvencPicParams.version = NV_ENC_PIC_PARAMS_VER;
    NvencPicParams.inputBuffer = NVInputResource.mappedResource;
    NvencPicParams.bufferFmt = NVInputResource.mappedBufferFmt;
    NvencPicParams.outputBitstream = NvencOutput;
    NvencPicParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    NvencPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
    NvencPicParams.completionEvent = nullptr;

    status = NVFunctions.nvEncEncodePicture((void*)NVEncoder, &NvencPicParams);
    NVFunctions.nvEncUnmapInputResource(NVEncoder, NVInputResource.mappedResource);
    if (status == NV_ENC_SUCCESS) {

        NVBitstreamLock = {};
        NVBitstreamLock.version = NV_ENC_LOCK_BITSTREAM_VER;
        NVBitstreamLock.outputBitstream = NvencOutput;
        NVBitstreamLock.doNotWait = false;

        status = NVFunctions.nvEncLockBitstream(NVEncoder, &NVBitstreamLock);
        if (status != NV_ENC_SUCCESS) {
            Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
            Logger::log(("\n RIP Output Lock " + std::to_string(status)).c_str());
        }

    } else {
        Logger::log(NVFunctions.nvEncGetLastErrorString(NVEncoder));
        Logger::log(("\n RIP Encoding " + std::to_string(status)).c_str());
    }
}

void NvencSession::NVUnlockBitStream()
{
    NVFunctions.nvEncUnlockBitstream(NVEncoder, NvencOutput);
}

void NvencSession::NVCleanup()
{

    status = NVFunctions.nvEncUnregisterResource(NVEncoder, NVRegisterResource.registeredResource);
    NVCHECK(status, "Nvenc Input Resource Failed To Unregister");

    status = NVFunctions.nvEncDestroyBitstreamBuffer(NVEncoder, NvencOutput);
    NVCHECK(status, "Nvenc Bit Stream Buffer Could Not Be Destroyed");

    status = NVFunctions.nvEncDestroyEncoder(NVEncoder);
    NVCHECK(status, "Nv Encoder Could Not Be Destroyed");
}

static void NvencOutputTest(NV_ENC_LOCK_BITSTREAM& NVBitstreamLock, const char* baseName)
{
    static uint64_t frameIndex = 0;

    if (!NVBitstreamLock.bitstreamBufferPtr || NVBitstreamLock.bitstreamSizeInBytes == 0) {
        Logger::log("NvencOutputTest: empty bitstream\n");
        return;
    }

    char fileName[512];
    std::snprintf(fileName,
                  sizeof(fileName),
                  "%s_%llu.h264",
                  baseName,
                  static_cast<unsigned long long>(frameIndex++));

    FILE* outFile = std::fopen(fileName, "wb");
    if (outFile) {
        std::fwrite(
            NVBitstreamLock.bitstreamBufferPtr, 1, NVBitstreamLock.bitstreamSizeInBytes, outFile);
        std::fclose(outFile);
    } else {
        Logger::log("Failed to open output file\n");
    }
}

