/*
* Copyright 2017-2024 NVIDIA Corporation.  All rights reserved.
*
* Please refer to the NVIDIA end user license agreement (EULA) associated
* with this source code for terms and conditions that govern your use of
* this software. Any use, reproduction, disclosure, or distribution of
* this software and related documentation outside the terms of the EULA
* is strictly prohibited.
*
*/

//---------------------------------------------------------------------------
//! \file AppDecUtils.h
//! \brief Header file containing definitions of miscellaneous functions used by Decode samples
//---------------------------------------------------------------------------

#pragma once
#include <sstream>
#include <iostream>

static void ShowHelpAndExit(const char *szBadOption, char *szOutputFileName, bool *pbVerbose, int *piD3d, bool *pbForce_zero_latency)
{
    std::ostringstream oss;
    bool bThrowError = false;
    if (szBadOption) {
        bThrowError = false;
        oss << "Error parsing \"" << szBadOption << "\"" << std::endl;
    }
    std::cout << "Options:" << std::endl
        << "-i                        Input file path" << std::endl
        << (szOutputFileName     ? "-o                        Output file path\n" : "")
        << "-gpu                      Ordinal of GPU to use" << std::endl
        << (pbVerbose            ? "-v                        Verbose message\n" : "")
        << (piD3d                ? "-d3d                      9 (default): display with D3D9; 11: display with D3D11\n" : "")
        << (pbForce_zero_latency ? "-force_zero_latency       Enable zero latency for All-Intra / IPPP streams. Do not use this flag if the stream contains B-frames.\n" : "")
        ;
    if (bThrowError) {
        throw std::invalid_argument(oss.str());
    }
    else {
        std::cout << oss.str();
        exit(0);
    }
}

static void ParseCommandLine(int argc, char *argv[], char *szInputFileName,
    char *szOutputFileName, int &iGpu, bool *pbVerbose = NULL, int *piD3d = NULL,
    bool *pbForce_zero_latency = NULL)
{
    std::ostringstream oss;
    int i;
    for (i = 1; i < argc; i++) {
        if (!_stricmp(argv[i], "-h")) {
            ShowHelpAndExit(NULL, szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
        }
        if (!_stricmp(argv[i], "-i")) {
            if (++i == argc) {
                ShowHelpAndExit("-i", szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o")) {
            if (++i == argc || !szOutputFileName) {
                ShowHelpAndExit("-o", szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit("-gpu", szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-v")) {
            if (!pbVerbose) {
                ShowHelpAndExit("-v", szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
            }
            *pbVerbose = true;
            continue;
        }
        if (!_stricmp(argv[i], "-d3d")) {
            if (++i == argc || !piD3d) {
                ShowHelpAndExit("-d3d", szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
            }
            *piD3d = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-force_zero_latency")) {
            if (!pbForce_zero_latency) {
                ShowHelpAndExit("-force_zero_latency", szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
            }
            *pbForce_zero_latency = true;
            continue;
        }
        ShowHelpAndExit(argv[i], szOutputFileName, pbVerbose, piD3d, pbForce_zero_latency);
    }
}

/**
*   @brief  Function to generate space-separated list of supported video surface formats
*   @param  nOutputFormatMask - Bit mask to represent supported cudaVideoSurfaceFormat in decoder
*   @param  OutputFormats     - Variable into which output string is written
*/
static void getOutputFormatNames(unsigned short nOutputFormatMask, char *OutputFormats)
{
    if (nOutputFormatMask == 0) {
        strcpy(OutputFormats, "N/A");
        return;
    }

    if (nOutputFormatMask & (1U << cudaVideoSurfaceFormat_NV12)) {
        strcat(OutputFormats, "NV12 ");
    }

    if (nOutputFormatMask & (1U << cudaVideoSurfaceFormat_P016)) {
        strcat(OutputFormats, "P016 ");
    }

    if (nOutputFormatMask & (1U << cudaVideoSurfaceFormat_YUV444)) {
        strcat(OutputFormats, "YUV444 ");
    }

    if (nOutputFormatMask & (1U << cudaVideoSurfaceFormat_YUV444_16Bit)) {
        strcat(OutputFormats, "YUV444P16 ");
    }

    if (nOutputFormatMask & (1U << cudaVideoSurfaceFormat_NV16)) {
        strcat(OutputFormats, "NV16 ");
    }

    if (nOutputFormatMask & (1U << cudaVideoSurfaceFormat_P216)) {
        strcat(OutputFormats, "P216 ");
    }
    return;
}

/**
*   @brief  Utility function to create CUDA context
*   @param  cuContext - Pointer to CUcontext. Updated by this function.
*   @param  iGpu      - Device number to get handle for
*/
static void createCudaContext(CUcontext* cuContext, int iGpu, unsigned int flags)
{
    CUdevice cuDevice = 0;
    ck(cuDeviceGet(&cuDevice, iGpu));
    char szDeviceName[80];
    ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
    std::cout << "GPU in use: " << szDeviceName << std::endl;
    ck(cuCtxCreate(cuContext, flags, cuDevice));
}

/**
*   @brief  Print decoder capabilities on std::cout
*/
static void ShowDecoderCapability()
{
    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    std::cout << "Decoder Capability" << std::endl << std::endl;
    
    struct caps {
        const char *aszCodecName;
        cudaVideoCodec aeCodec;
        cudaVideoChromaFormat aeChromaFormat;
        int anBitDepthMinus8;
    };
    
    caps queryList[] = {
        {"JPEG",     cudaVideoCodec_JPEG,     cudaVideoChromaFormat_420,          0},
        {"MPEG1",    cudaVideoCodec_MPEG1,    cudaVideoChromaFormat_420,          0},
        {"MPEG2",    cudaVideoCodec_MPEG2,    cudaVideoChromaFormat_420,          0},
        {"MPEG4",    cudaVideoCodec_MPEG4,    cudaVideoChromaFormat_420,          0},
        {"H264",     cudaVideoCodec_H264,     cudaVideoChromaFormat_420,          0},
        {"H264",     cudaVideoCodec_H264,     cudaVideoChromaFormat_420,          2},
        {"H264",     cudaVideoCodec_H264,     cudaVideoChromaFormat_422,          0},
        {"H264",     cudaVideoCodec_H264,     cudaVideoChromaFormat_422,          2},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_420,          0},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_420,          2},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_420,          4},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_422,          0},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_422,          2},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_422,          4},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_444,          0},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_444,          2},
        {"HEVC",     cudaVideoCodec_HEVC,     cudaVideoChromaFormat_444,          4},
        {"VC1",      cudaVideoCodec_VC1,      cudaVideoChromaFormat_420,          0},
        {"VP8",      cudaVideoCodec_VP8,      cudaVideoChromaFormat_420,          0},
        {"VP9",      cudaVideoCodec_VP9,      cudaVideoChromaFormat_420,          0},
        {"VP9",      cudaVideoCodec_VP9,      cudaVideoChromaFormat_420,          2},
        {"VP9",      cudaVideoCodec_VP9,      cudaVideoChromaFormat_420,          4},
        {"AV1",      cudaVideoCodec_AV1,      cudaVideoChromaFormat_420,          0},
        {"AV1",      cudaVideoCodec_AV1,      cudaVideoChromaFormat_420,          2},
        {"AV1",      cudaVideoCodec_AV1,      cudaVideoChromaFormat_Monochrome,   0},
        {"AV1",      cudaVideoCodec_AV1,      cudaVideoChromaFormat_Monochrome,   2},
    };

    const char *aszChromaFormat[] = { "4:0:0", "4:2:0", "4:2:2", "4:4:4" };
    char strOutputFormats[64];

    for (int iGpu = 0; iGpu < nGpu; iGpu++) {

        CUcontext cuContext = NULL;
        createCudaContext(&cuContext, iGpu, 0);

        for (int i = 0; i < sizeof(queryList) / sizeof(queryList[0]); i++) {

            CUVIDDECODECAPS decodeCaps = {};
            decodeCaps.eCodecType = queryList[i].aeCodec;
            decodeCaps.eChromaFormat = queryList[i].aeChromaFormat;
            decodeCaps.nBitDepthMinus8 = queryList[i].anBitDepthMinus8;

            cuvidGetDecoderCaps(&decodeCaps);

            strOutputFormats[0] = '\0';
            getOutputFormatNames(decodeCaps.nOutputFormatMask, strOutputFormats);

            // setw() width = maximum_width_of_string + 2 spaces
            std::cout << "Codec  " << std::left << std::setw(7) << queryList[i].aszCodecName <<
                "BitDepth  " << std::setw(4) << decodeCaps.nBitDepthMinus8 + 8 <<
                "ChromaFormat  " << std::setw(7) << aszChromaFormat[decodeCaps.eChromaFormat] <<
                "Supported  " << std::setw(3) << (int)decodeCaps.bIsSupported <<
                "MaxWidth  " << std::setw(7) << decodeCaps.nMaxWidth <<
                "MaxHeight  " << std::setw(7) << decodeCaps.nMaxHeight <<
                "MaxMBCount  " << std::setw(10) << decodeCaps.nMaxMBCount <<
                "MinWidth  " << std::setw(5) << decodeCaps.nMinWidth <<
                "MinHeight  " << std::setw(5) << decodeCaps.nMinHeight <<
                "SurfaceFormat  " << std::setw(11) << strOutputFormats << std::endl;
        }

        std::cout << std::endl;

        ck(cuCtxDestroy(cuContext));
    }
}
