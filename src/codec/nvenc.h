#ifndef NVENCODER_H
#define NVENCODER_H

#include <string>
#include <vector>
#include "OmniLogger.h"
#include "NvencTypes.h"
#include <d3d11.h>
#include <nvEncodeAPI.h>

#pragma comment(lib, "nvencodeapi.lib")



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

	static void GetSupportedCodecGUIDs(void* NVEncoder, NV_ENCODE_API_FUNCTION_LIST& NVFunctions_);
	void GetAvailablePresetGUIDs(void* NVEncoder);
	void GetAvailableProfileGUIDs(void* NVEncoder, GUID NvencCodecGUID);
	void GetSupportedInputFormats(void* NVEncoder, GUID NvencCodecGUID);


};


class NvencSession
{
public:
	void* NVEncoder = nullptr;

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
