#include "nvenc.h"


void NVENCODER::LoadNvencAPI() {
	HMODULE API_Handle = LoadLibrary("nvencodeapi64.dll");
	if (!API_Handle) {
		OutputDebugString("\n LoadLibrary Died!!");
		return;
	}

	typedef NVENCSTATUS(NVENCAPI* PFN_NvEncodeAPICreateInstance)(NV_ENCODE_API_FUNCTION_LIST*);

	auto NvEncodeAPICreateInstance = (PFN_NvEncodeAPICreateInstance)GetProcAddress(API_Handle, "NvEncodeAPICreateInstance");
	if (!NvEncodeAPICreateInstance) {
		OutputDebugString("Encode API Instance Creation Failed -_- \n");
		return;
	}
	
	NVFunctions.version = NV_ENCODE_API_FUNCTION_LIST_VER;
	status = NvEncodeAPICreateInstance(&NVFunctions);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugString("Encode API Function Filling Failed -_- \n");
	}
}

void NVENCODER::OpenNvEncSession() {
	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS NVSessionParams = {};
	NVSessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
	NVSessionParams.apiVersion = NVENCAPI_VERSION;
	NVSessionParams.device = D3DDevice;
	NVSessionParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
	status = NVFunctions.nvEncOpenEncodeSessionEx(&NVSessionParams, &NVEncoder);

	if (status != NV_ENC_SUCCESS || NVEncoder == nullptr) {
		OutputDebugStringA(NVFunctions.nvEncGetLastErrorString(NVEncoder));
		OutputDebugString("Encoder did not feel like coming home. Prolly \n");
	}

	
}

void NVENCODER::LoadDefaultInitParams() {
	NVPresetConfig.version = NV_ENC_PRESET_CONFIG_VER;
	NVPresetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
	NVFunctions.nvEncGetEncodePresetConfig(NVEncoder, NvencEncodeGUID, NvencPresetGUID, &NVPresetConfig);

	NVPresetConfig.presetCfg.gopLength = 1;
	NVPresetConfig.presetCfg.encodeCodecConfig.h264Config.idrPeriod = 1;
	NVPresetConfig.presetCfg.encodeCodecConfig.h264Config.repeatSPSPPS = 1;

	NvInitParams.encodeConfig = &NVPresetConfig.presetCfg;


	NvInitParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
	NvInitParams.bufferFormat = NvencBufferFormat;
	NvInitParams.encodeGUID = NvencEncodeGUID;
	NvInitParams.presetGUID = NvencPresetGUID;
	NvInitParams.tuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
	NvInitParams.encodeWidth = bufferWidth;
	NvInitParams.encodeHeight = bufferHeight;
	NvInitParams.darWidth = bufferWidth;
	NvInitParams.darHeight = bufferHeight;
	NvInitParams.frameRateNum = 60;
	NvInitParams.frameRateDen = 1;
	NvInitParams.enablePTD = 1;
	NvInitParams.enableEncodeAsync = 0;

}

void NVENCODER::NVEncoderInit() {
	status = NVFunctions.nvEncInitializeEncoder(NVEncoder, &NvInitParams);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugStringA(NVFunctions.nvEncGetLastErrorString(NVEncoder));
		OutputDebugString((" RIP Encoder Init " + std::to_string(status) + "\n").c_str());
	}
}

void NVENCODER::ResgisterResource(ID3D11Texture2D* inputResource) {
	NV_ENC_REGISTER_RESOURCE NVRegisterResource = { };
	NVRegisterResource.version = NV_ENC_REGISTER_RESOURCE_VER;
	NVRegisterResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
	NVRegisterResource.resourceToRegister = inputResource;
	NVRegisterResource.width = bufferWidth;
	NVRegisterResource.height = bufferHeight;
	NVRegisterResource.bufferFormat = NvencBufferFormat;
	NVRegisterResource.pitch = bufferWidth * 4;

	status = NVFunctions.nvEncRegisterResource(NVEncoder, &NVRegisterResource);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugStringA(NVFunctions.nvEncGetLastErrorString(NVEncoder));
		OutputDebugString(("RIP Encoder Input Resource Marriage \n" + std::to_string(status)).c_str());
	}
}

void NVENCODER::CreateBitStream() {
	NVOutputBufferDesc.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;

	status = NVFunctions.nvEncCreateBitstreamBuffer(NVEncoder, &NVOutputBufferDesc);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugString(("RIP Encode Output Stream Buffer \n" + std::to_wstring(status)).c_str());
	}
}