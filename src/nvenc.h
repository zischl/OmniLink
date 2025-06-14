#ifndef NVENCODER_H
#define NVENCODER_H

#include <string>
#include <d3d11.h>
#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")

class NVENCODER {
private:
	void* _D3DDevice;
	NVENCSTATUS status;

	UINT bufferWidth;
	UINT bufferHeight;

	GUID NvencEncodeGUID = NV_ENC_CODEC_H264_GUID;
	GUID NvencPresetGUID = NV_ENC_PRESET_P1_GUID;
	GUID NvencProfileGUID = NV_ENC_H264_PROFILE_HIGH_GUID;

	NV_ENC_BUFFER_FORMAT NvencBufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;

	NV_ENC_INITIALIZE_PARAMS NvInitParams = {};
	NV_ENC_PRESET_CONFIG NVPresetConfig = {};
	NV_ENC_CONFIG_H264 NVH264Cfg = {};
	NV_ENC_CONFIG NVInitConfig = {};

	NV_ENC_REGISTER_RESOURCE NVRegisterResource = { };

	NV_ENC_CREATE_BITSTREAM_BUFFER NVOutputBufferDesc = { };
	NV_ENC_OUTPUT_PTR NvencOutput;

public:
	NV_ENCODE_API_FUNCTION_LIST NVFunctions = { };
	void* NVEncoder;

	NVENCODER(void* D3DDevice, ID3D11Texture2D* inputResource, UINT encodeWidth, UINT encodeHeight) {
		_D3DDevice = D3DDevice;
		bufferWidth = encodeWidth;
		bufferHeight = encodeHeight;
		LoadNvencAPI();
		OpenNvEncSession();
		LoadDefaultInitParams();
		NVEncoderInit();
		RegisterResource(inputResource);
		CreateBitStream();
	}

	void LoadNvencAPI();
	void OpenNvEncSession();
	void LoadDefaultInitParams();
	void NVEncoderInit();
	void RegisterResource(ID3D11Texture2D* inputResource);
	void CreateBitStream();
	void Encode();


	NV_ENC_OUTPUT_PTR getBitstream() const { return NvencOutput; }
	NV_ENC_REGISTER_RESOURCE getRegisteredResource() const { return NVRegisterResource; }

};

#endif