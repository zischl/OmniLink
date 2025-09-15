#ifndef NVENCODER_H
#define NVENCODER_H

#include <string>
#include <vector>

#include <d3d11.h>
#include <nvEncodeAPI.h>

#pragma comment(lib, "nvencodeapi.lib")


class NVENCODER {
private:
	void* _D3DDevice;
	void* NVEncoder;

	NV_ENCODE_API_FUNCTION_LIST NVFunctions = {};
	NVENCSTATUS status;

	UINT bufferWidth;
	UINT bufferHeight;

	GUID NvencEncodeGUID = NV_ENC_CODEC_H264_GUID;
	GUID NvencPresetGUID = NV_ENC_PRESET_P7_GUID;
	GUID NvencProfileGUID = NV_ENC_H264_PROFILE_HIGH_GUID;

	NV_ENC_BUFFER_FORMAT NvencBufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;

	NV_ENC_INITIALIZE_PARAMS NvInitParams = {};
	NV_ENC_CONFIG NVInitConfig = {};
	NV_ENC_PRESET_CONFIG NVPresetConfig = {};

	NV_ENC_REGISTER_RESOURCE NVRegisterResource = {};

	NV_ENC_CREATE_BITSTREAM_BUFFER NVOutputBufferDesc = {};
	NV_ENC_OUTPUT_PTR NvencOutput;

	size_t* outputSize;
	uint8_t* output;

	bool EncodeStatus = false;
	

public:
	NVENCODER(void* D3DDevice, ID3D11Texture2D* inputResource, UINT encodeWidth, UINT encodeHeight);

	void LoadNvEncodeAPI();

	void OpenNvEncSession();
	void LoadDefaultInitParams();
	void NVEncoderInit();
	void RegisterResource(ID3D11Texture2D* inputResource);
	void CreateBitStream();
	void Encode();
	void NVUnlockBitStream();
	void NVCleanup();

	void GetSupportedCodecGUIDs();
	void GetAvailablePresetGUIDs();
	void GetAvailableProfileGUIDs();
	void GetSupportedInputFormats();

	NV_ENC_LOCK_BITSTREAM NVBitstreamLock = { };
	NV_ENC_OUTPUT_PTR getBitstream() const { return NvencOutput; }
	NV_ENC_REGISTER_RESOURCE getRegisteredResource() const { return NVRegisterResource; }

};

#endif