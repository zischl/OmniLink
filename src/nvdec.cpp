#include "nvdec.h"

NVDecoder::NVDecoder(UINT Width, UINT Height, ID3D11Texture2D* OutputTexture) {
	width = Width;
	height = Height;

	OutputBuffer = OutputTexture;

	CResult = InitializeNVDEC();
	if (CResult != CUDA_SUCCESS) {
		OutputDebugString(("\nDecoder Init Failed The One Job He Had Successfuly!" + std::to_string(CResult)).c_str());
	}

	cuCtxPushCurrent(CudaContext);

	CResult = cuGraphicsD3D11RegisterResource(&CudaOutputResource, OutputBuffer, 0);
	if (CResult != CUDA_SUCCESS || CudaOutputResource == NULL) {
		OutputDebugString(("\nDecoder Output Texture Registration Failed Successfuly!" + std::to_string(CResult)).c_str());
	}
}

CUresult NVDecoder::InitializeNVDEC(){
    NvDecAPIHandle = LoadLibrary("nvcuvid.dll");
	if (!NvDecAPIHandle) {
		OutputDebugString("\n LoadLibrary Died!!\n");
		return CUDA_ERROR_UNKNOWN;
	}

	NvCudaAPIHandle = LoadLibrary("nvcuda.dll");
	if (!NvCudaAPIHandle) {
		OutputDebugString("\n LoadLibrary Died!!\n");
		return CUDA_ERROR_UNKNOWN;
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

	if (CResult != CUDA_SUCCESS) { OutputDebugString(("\nWHICH GET PROC FAILED ?_?" + std::to_string(CResult) + "\n").c_str()); }

	cuInit(0);
	cuDeviceGet(&CudaDevice, 0);
	cuCtxCreate(&CudaContext, 0, CudaDevice);
	
	
	CudaDecoderInfo.CodecType = cudaVideoCodec_H264;
	CudaDecoderInfo.ulWidth = width;
	CudaDecoderInfo.ulHeight = height;
	CudaDecoderInfo.ulTargetWidth = width;
	CudaDecoderInfo.ulTargetHeight = height;
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

	CResult = cuvidCreateDecoder(&CudaDecoder, &CudaDecoderInfo);
	if (CResult != CUDA_SUCCESS || CudaDecoder == NULL) { OutputDebugString(("\nDecoder Creation Failed Successfuly !"+std::to_string(CResult)).c_str()); }


	CudaParserParams.CodecType = cudaVideoCodec_H264;
	CudaParserParams.ulMaxNumDecodeSurfaces = 8;
	CudaParserParams.ulMaxDisplayDelay = 0;
	CudaParserParams.pUserData = this;
	CudaParserParams.pfnSequenceCallback = ParserSequenceCallback;
	CudaParserParams.pfnDecodePicture = PictureDecodeCallback;
	CudaParserParams.pfnDisplayPicture = PictureOutputCallback;


	CResult = cuvidCreateVideoParser(&CudaParser, &CudaParserParams);
	if (CResult != CUDA_SUCCESS) { OutputDebugString(("\nVideo Parser Creation Failed Spectacularly !" + std::to_string(CResult)).c_str()); }

	return CResult;
}


void NVDecoder::NVDecode(const unsigned char* data, unsigned long size) {
	CUVIDSOURCEDATAPACKET DataPacket = {};
	DataPacket.payload = data;
	DataPacket.payload_size = size;
	CResult = cuvidParseVideoData(CudaParser, &DataPacket);
	if (CResult != CUDA_SUCCESS) { OutputDebugString(("\nParser Failed Successfuly !, you had one job..." + std::to_string(CResult)).c_str()); }
}

int CUDAAPI NVDecoder::ParserSequenceCallback(void* instanceData, CUVIDEOFORMAT* CuDecoderInfo) {

	NVDecoder* instance = static_cast<NVDecoder*>(instanceData);

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
		OutputDebugString("ehfisehgiseufhehg!");
	}

	
	instance->CResult = instance->cuvidReconfigureDecoder(instance->CudaDecoder, &ReConfig);
	if (instance->CResult != CUDA_SUCCESS) {
		OutputDebugString(("\nDecoder Reconfiguration Failed The One Job He Had Successfuly (ignore 1 time)!" + std::to_string(instance->CResult)).c_str());
		return 1;
	}

	return 1;
}


int CUDAAPI NVDecoder::PictureDecodeCallback(void* instanceData, CUVIDPICPARAMS* DecoderPicParams) {

	NVDecoder* instance = static_cast<NVDecoder*>(instanceData);
	
	instance->CResult = instance->cuvidDecodePicture(instance->CudaDecoder, DecoderPicParams);
	if (instance->CResult != CUDA_SUCCESS) {
		OutputDebugString(("\nDecoder Failed The One Job He Had Successfuly !" + std::to_string(instance->CResult)).c_str());
		return 0;
	}
	return 1;

}


int CUDAAPI NVDecoder::PictureOutputCallback(void* instanceData, CUVIDPARSERDISPINFO* pDispInfo) {

	NVDecoder* instance = static_cast<NVDecoder*>(instanceData);

	CUVIDPROCPARAMS procParams = {};
	procParams.progressive_frame = 1;
	procParams.top_field_first = 0;
	procParams.unpaired_field = 0;


	cuCtxPushCurrent(instance->CudaContext);

	CUdeviceptr dpSrcFrame = 0;
	unsigned int pitch = 0;
	instance->CResult = instance->cuvidMapVideoFrame(instance->CudaDecoder, pDispInfo->picture_index, &dpSrcFrame, &pitch, &procParams);
	if (instance->CResult != CUDA_SUCCESS){
		OutputDebugString(("\nDecoder Output Callback Failed Successfuly !" + std::to_string(instance->CResult)).c_str());
		return 0;
	}

	//######################################################################################################
	//OutputDebugString(("\nI MADwwwwwwwE IT HERE !" + std::to_string(dpSrcFrame)).c_str());

	

	instance->CResult = instance->cuGraphicsMapResources(1, &instance->CudaOutputResource, 0);
	if (instance->CResult != CUDA_SUCCESS || instance->CudaOutputResource == nullptr) {
		OutputDebugString(("\nTexture Resource Mapping Failed Successfuly !" + std::to_string(instance->CResult)).c_str());
		return 0;
	}
	
	CUarray MappedResArray;

	instance->CResult = instance->cuGraphicsSubResourceGetMappedArray(&MappedResArray, instance->CudaOutputResource, 0, 0);
	if (instance->CResult != CUDA_SUCCESS) {
		OutputDebugString(("\nTexture Resource Mapped Array Y Retrieval Failed Successfuly !" + std::to_string(instance->CResult)).c_str());
		return 0;
	}

	cudaSurfaceObject_t OutputSurface;
	cudaResourceDesc OutputResDesc = {};
	OutputResDesc.resType = cudaResourceTypeArray;
	OutputResDesc.res.array.array = reinterpret_cast<cudaArray*>(MappedResArray);

	cudaError CudaResult = cudaCreateSurfaceObject(&OutputSurface, &OutputResDesc);
	if (CudaResult != cudaSuccess) {
		OutputDebugString(("\nTexture Surface Object Creation Failed Successfuly !" + std::to_string(CudaResult)).c_str());
	}


	const uint8_t* NV12Buffer = reinterpret_cast<const uint8_t*>(dpSrcFrame);
	Cast2BGRA(NV12Buffer, instance->width, instance->height, OutputSurface, pitch);



	CUDA_ARRAY_DESCRIPTOR arrayDesc = {};
	cuArrayGetDescriptor_v2(&arrayDesc, MappedResArray);
	//OutputDebugString(std::to_string(arrayDesc.Height).c_str());



	instance->CResult = instance->cuGraphicsUnmapResources(1, &instance->CudaOutputResource, 0);
	if (instance->CResult != CUDA_SUCCESS) {
		OutputDebugString(("\nTexture Resource Unmapping Failed Successfuly !" + std::to_string(instance->CResult)).c_str());
		return 0;
	}

	//######################################################################################################

	instance->CResult = instance->cuvidUnmapVideoFrame(instance->CudaDecoder, dpSrcFrame);
	if (instance->CResult != CUDA_SUCCESS) { OutputDebugString(("\nDecoder Output Unmap Failed Successfuly !" + std::to_string(instance->CResult)).c_str()); }
	return 1;
}

void NVDecoder::Cleanup() {
	CResult = cuvidDestroyVideoParser(CudaParser);
	if (CResult != CUDA_SUCCESS) { OutputDebugString(("\nCuda Decoder Cleanup Failed Successfuly !" + std::to_string(CResult)).c_str()); }
}


void NVDecoder::CompatibilityCheck() {
	CUVIDDECODECAPS CudaCaps = {};
	CudaCaps.eCodecType = cudaVideoCodec_H264;
	CudaCaps.eChromaFormat = cudaVideoChromaFormat_420;
	CudaCaps.nBitDepthMinus8 = 0;

	cuvidGetDecoderCaps(&CudaCaps);
}