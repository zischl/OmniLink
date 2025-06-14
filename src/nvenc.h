#ifndef NVENCODER_H
#define NVENCODER_H

#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")

class NVENCODER {
private:
	HMODULE API_Handle;
public:
	void LoadNvencApi();
};

#endif