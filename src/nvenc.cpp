#include "nvenc.h"


void NVENCODER::LoadNvencApi() {
	API_Handle = LoadLibrary("nvencodeapi64.dll");
	if (!API_Handle) {
		OutputDebugString("\n LoadLibrary Died!!");
	}

}

