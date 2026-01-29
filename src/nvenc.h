#ifndef NVENCODER_H
#define NVENCODER_H

#include <string>
#include <vector>
#include "OmniLogger.h"

#include <d3d11.h>
#include <nvEncodeAPI.h>

#pragma comment(lib, "nvencodeapi.lib")

struct NVec2U {
	size_t Width = 1920;
	size_t Height = 1080;
};

struct NvencResourceRegConfig {
	NVec2U Dimensions = {};
	NV_ENC_BUFFER_FORMAT NvencBufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
};

struct NvencStaticConfig : NvencResourceRegConfig
{
	GUID NvencCodecGUID = NV_ENC_CODEC_H264_GUID;
	GUID NvencProfileGUID = NV_ENC_H264_PROFILE_HIGH_GUID;
	NV_ENC_TUNING_INFO NvencTuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
};

struct NvencStaticConfigEx : NvencStaticConfig {
	int maxReferenceFrames;
	bool enableBFrames;
	bool enableSlicingMode;
};

struct NvencLiveConfig {
	GUID NvencPresetGUID = NV_ENC_PRESET_P7_GUID;
	int avgBitrate;
	int maxBitrate;
};

struct NvencLiveConfigEx : NvencLiveConfig {
	uint32_t FrameRateNum = 60;
	uint32_t FrameRateDen = 60;
	uint32_t EnablePTD = 1;
};

struct NvencInitConfig : NvencLiveConfigEx, NvencStaticConfig {};


class NVENCODER {
private:

	NVENCSTATUS status;

	size_t* outputSize;
	uint8_t* output;

	bool EncodeStatus = false;


public:
	NV_ENCODE_API_FUNCTION_LIST NVFunctions = {};


	NVENCODER();
	~NVENCODER();

	void LoadNvEncodeAPI();

	void GetSupportedCodecGUIDs(void* NVEncoder);
	void GetAvailablePresetGUIDs(void* NVEncoder);
	void GetAvailableProfileGUIDs(void* NVEncoder, GUID NvencCodecGUID);
	void GetSupportedInputFormats(void* NVEncoder, GUID NvencCodecGUID);


};


class NvencSession
{
public:
	void* NVEncoder;

	NV_ENC_REGISTER_RESOURCE NVRegisterResource = {};
	NV_ENC_OUTPUT_PTR NvencOutput;
	NV_ENC_LOCK_BITSTREAM NVBitstreamLock = { };

	NvencSession(void* D3DDevice, NV_ENCODE_API_FUNCTION_LIST& NVFunctions_, ID3D11Texture2D* inputResource, UINT encodeWidth, UINT encodeHeight);

	void Encode();
	void NVUnlockBitStream();
	void NVCleanup();

private:
	NVENCSTATUS status;
	NV_ENCODE_API_FUNCTION_LIST& NVFunctions;

	NV_ENC_CREATE_BITSTREAM_BUFFER NVOutputBufferDesc = {};

	void OpenNvEncSession(void* D3DDevice);
	void LoadDefaultInitParams(NV_ENC_INITIALIZE_PARAMS& NvInitParams, NvencInitConfig& config);
	void NVEncoderInit(NV_ENC_INITIALIZE_PARAMS& NvInitParams);

	void RegisterResource(ID3D11Texture2D* inputResource, NvencResourceRegConfig& Config);

	void CreateBitStream();
	

	NV_ENC_OUTPUT_PTR getBitstream() const { return NvencOutput; }
	NV_ENC_REGISTER_RESOURCE getRegisteredResource() const { return NVRegisterResource; }

};
#endif
