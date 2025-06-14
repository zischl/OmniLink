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
	NVSessionParams.device = _D3DDevice;
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

void NVENCODER::RegisterResource(ID3D11Texture2D* inputResource) {
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
		OutputDebugString(("RIP Encode Output Stream Buffer \n" + std::to_string(status)).c_str());
	}
	NvencOutput = NVOutputBufferDesc.bitstreamBuffer;
}

void NVENCODER::Encode() {
	NV_ENC_MAP_INPUT_RESOURCE NVInputResource = { };
	NVInputResource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
	NVInputResource.registeredResource = NVRegisterResource.registeredResource;
	status = NVFunctions.nvEncMapInputResource(NVEncoder, &NVInputResource);
	if (status != NV_ENC_SUCCESS) {
		OutputDebugStringA(NVFunctions.nvEncGetLastErrorString(NVEncoder));
		OutputDebugString(("RIP Input Resource Map \n" + std::to_string(status)).c_str());
	}




	NV_ENC_PIC_PARAMS NvencPicParams = { };
	memset(&NvencPicParams, 0, sizeof(NV_ENC_PIC_PARAMS));

	NvencPicParams.version = NV_ENC_PIC_PARAMS_VER;
	NvencPicParams.inputWidth = bufferWidth;
	NvencPicParams.inputHeight = bufferHeight;
	NvencPicParams.inputBuffer = NVInputResource.mappedResource;
	NvencPicParams.bufferFmt = NVInputResource.mappedBufferFmt;
	NvencPicParams.outputBitstream = NvencOutput;
	NvencPicParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	NvencPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
	NvencPicParams.completionEvent = nullptr;
	NvencPicParams.inputPitch = bufferWidth * 4;




	status = NVFunctions.nvEncEncodePicture((void*)NVEncoder, &NvencPicParams);
	NVFunctions.nvEncUnmapInputResource(NVEncoder, NVInputResource.mappedResource);
	if (status == NV_ENC_SUCCESS) {

		NV_ENC_LOCK_BITSTREAM NVBitstreamLock = { };
		NVBitstreamLock.version = NV_ENC_LOCK_BITSTREAM_VER;
		NVBitstreamLock.outputBitstream = NvencOutput;
		NVBitstreamLock.doNotWait = false;

		status = NVFunctions.nvEncLockBitstream(NVEncoder, &NVBitstreamLock);
		if (status != NV_ENC_SUCCESS) {
			OutputDebugStringA(NVFunctions.nvEncGetLastErrorString(NVEncoder));
			OutputDebugString(("\n RIP Output Lock " + std::to_string(status)).c_str());
		}

		size_t bitstreamSize = NVBitstreamLock.bitstreamSizeInBytes;
		OutputDebugString((std::to_string(bitstreamSize) + "\n").c_str());

		

		NVFunctions.nvEncUnlockBitstream(NVEncoder, NvencOutput);
	}
	else {
		OutputDebugStringA(NVFunctions.nvEncGetLastErrorString(NVEncoder));
		OutputDebugString(("\n RIP Encoding " + std::to_string(status)).c_str());
	}
}