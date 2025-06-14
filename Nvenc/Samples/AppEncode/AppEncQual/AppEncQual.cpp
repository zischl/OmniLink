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

/**
* This sample application demonstrates an Iterative Encoder implementation.
* A constant quality mode is implemented where the user is able to specify a 
* minimum and maximum PSNR-Y as well as maximum number of iterations per frame.
* The Iterative Encoder will:  
* 1. Interrupt the encoder state after each encoded frame; 
* 2. Check the Reconstructed frame's PSNR-Y (Reconstructed Frame Output API); 
* 3. Compare against the user defined range of desired PSNRs; 
* 4. Adjust the QP/CQ paramter for the next iteration (Reconfigure API);
* 5. After the desired PSNR range or maximum number of iterations is reached, the
* encoder state is advanced and the next frame is encoded
* This sample is compatible with rate controls Constant QP and VBR Constant Quality.
* The QP/CQ parameter is adjusted based on qpDelta input paramter (default: 1)   
*/

#include <cuda.h>
#include <iostream>
#include <iomanip>
#include <memory>
#include <stdint.h>
#include "NvEncoder/NvEncoderCuda.h"
#include "NvEncoder/NvEncoderCudaIterative.h"
#include "../Utils/NvEncoderCLIOptions.h"
#include "../Utils/NvCodecUtils.h"

simplelogger::Logger *logger = simplelogger::LoggerFactory::CreateConsoleLogger();

void ShowHelpAndExit(const char *szBadOption = NULL)
{
    bool bThrowError = false;
    std::ostringstream oss;
    if (szBadOption)
    {
        bThrowError = true;
        oss << "Error parsing \"" << szBadOption << "\"" << std::endl;
    }
    oss << "Options:" << std::endl
        << "-i           Input file path" << std::endl
        << "-o           Output file path" << std::endl
        << "-s           Input resolution in this form: WxH" << std::endl
        << "-if          Input format: iyuv nv12 yv12 nv16 p210 yuv444" << std::endl
        << "-gpu         Ordinal of GPU to use" << std::endl
        << "-maxiter     Maximum number of iterations per frame for Iterative Encoder" << std::endl
        << "-minpsnr     Minimum target PSNR" << std::endl
        << "-maxpsnr     Maximum target PSNR" << std::endl
        << "-qd          Delta QP/CQ adjustment" << std::endl
        ;
    oss << NvEncoderInitParam().GetHelpMessage(false, false, true);
    if (bThrowError)
    {
        throw std::invalid_argument(oss.str());
    }
    else
    {
        std::cout << oss.str();
        exit(0);
    }
}

void ValidateQualityRange(uint32_t minQual, uint32_t maxQual)
{
    if (minQual >= maxQual) {
        std::ostringstream err;
        err << "Please specify a minimum PSNR lower than the maximuim PSNR. Current minimum PSNR is " << minQual << " and maximum PSNR is " << maxQual << std::endl;
        throw std::invalid_argument(err.str());
    }
}

void ParseCommandLine(int argc, char *argv[], char *szInputFileName, int &nWidth, int &nHeight,
    NV_ENC_BUFFER_FORMAT &eFormat, char *szOutputFileName, NvEncoderInitParam &initParam,
    int &iGpu, uint32_t &nNumIterations, uint32_t &nMinTargetMetric, uint32_t &nMaxTargetMetric, uint32_t &nQPDelta)
{
    std::ostringstream oss;
    int i;
    for (i = 1; i < argc; i++)
    {
        if (!_stricmp(argv[i], "-h")) 
        {
            ShowHelpAndExit();
        }
        if (!_stricmp(argv[i], "-i"))
        {
            if (++i == argc) 
            {
                ShowHelpAndExit("-i");
            }
            sprintf(szInputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-o"))
        {
            if (++i == argc)
            {
                ShowHelpAndExit("-o");
            }
            sprintf(szOutputFileName, "%s", argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-s"))
        {
            if (++i == argc || 2 != sscanf(argv[i], "%dx%d", &nWidth, &nHeight))
            {
                ShowHelpAndExit("-s");
            }
            continue;
        }
        std::vector<std::string> vszFileFormatName =
		{
			"iyuv", "nv12", "yv12", "nv16", "p210", "yuv444",
		};
		NV_ENC_BUFFER_FORMAT aFormat[] =
		{
			NV_ENC_BUFFER_FORMAT_IYUV,
			NV_ENC_BUFFER_FORMAT_NV12,
			NV_ENC_BUFFER_FORMAT_YV12,
            NV_ENC_BUFFER_FORMAT_NV16,
            NV_ENC_BUFFER_FORMAT_P210,
			NV_ENC_BUFFER_FORMAT_YUV444,
		};
        if (!_stricmp(argv[i], "-if"))
        {
            if (++i == argc) 
            {
                ShowHelpAndExit("-if");
            }
            auto it = find(vszFileFormatName.begin(), vszFileFormatName.end(), argv[i]);
            if (it == vszFileFormatName.end())
            {
                ShowHelpAndExit("-if");
            }
            eFormat = aFormat[it - vszFileFormatName.begin()];
            continue;
        }
        if (!_stricmp(argv[i], "-gpu")) {
            if (++i == argc) {
                ShowHelpAndExit("-gpu");
            }
            iGpu = atoi(argv[i]);
            continue;
        }
        if (!_stricmp(argv[i], "-maxiter"))
		{
			if (++i == argc)
			{
				ShowHelpAndExit("-maxiter");
			}
			nNumIterations = atoi(argv[i]);
			continue;
		}
		if (!_stricmp(argv[i], "-minpsnr"))
		{
			if (++i == argc)
			{
				ShowHelpAndExit("-minpsnr");
			}
			nMinTargetMetric = atoi(argv[i]);
			continue;
		}
		if (!_stricmp(argv[i], "-maxpsnr"))
		{
			if (++i == argc)
			{
				ShowHelpAndExit("-maxpsnr");
			}
			nMaxTargetMetric = atoi(argv[i]);
			continue;
		}
        if (!_stricmp(argv[i], "-qd"))
		{
			if (++i == argc)
			{
				ShowHelpAndExit("-qd");
			}
			nQPDelta = atoi(argv[i]);
			continue;
		}
        // Regard as encoder parameter
        if (argv[i][0] != '-') {
            ShowHelpAndExit(argv[i]);
        }
        oss << argv[i] << " ";
        while (i + 1 < argc && argv[i + 1][0] != '-') {
            oss << argv[++i] << " ";
        }
    }
    initParam = NvEncoderInitParam(oss.str().c_str());
}

template<class EncoderClass>
void InitializeEncoder(EncoderClass &pEnc, NvEncoderInitParam encodeCLIOptions, NV_ENC_BUFFER_FORMAT eFormat, uint32_t nIterations)
{
	NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
	NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };

	initializeParams.encodeConfig = &encodeConfig;
	pEnc->CreateDefaultEncoderParams(&initializeParams, encodeCLIOptions.GetEncodeGUID(), encodeCLIOptions.GetPresetGUID(), encodeCLIOptions.GetTuningInfo());
	encodeCLIOptions.SetInitParams(&initializeParams, eFormat);

	initializeParams.enableReconFrameOutput = true; // NVENC Reconstructed Frame Output API
	initializeParams.enableOutputStats = true; // Encoded Frame Stats API
	initializeParams.numStateBuffers = nIterations; // Iterative encoding API
	pEnc->CreateEncoder(&initializeParams);
}

void EncQual(char *szInFilePath, char *szOutFilePath, int nWidth, int nHeight, NV_ENC_BUFFER_FORMAT eFormat, int iGpu, NvEncoderInitParam &encodeCLIOptions, uint32_t nNumIterations, uint32_t minTargetQuality, uint32_t maxTargetQuality, uint32_t nQPDelta)
{

    // Open input file
    std::ifstream fpIn(szInFilePath, std::ifstream::in | std::ifstream::binary);
    if (!fpIn)
    {
        std::ostringstream err;
        err << "Unable to open input file: " << szInFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    // Open output file
    std::ofstream fpOut(szOutFilePath, std::ios::out | std::ios::binary);
    if (!fpOut)
    {
        std::ostringstream err;
        err << "Unable to open output file: " << szOutFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }

    ck(cuInit(0));
    int nGpu = 0;
    ck(cuDeviceGetCount(&nGpu));
    if (iGpu < 0 || iGpu >= nGpu)
    {
        std::cout << "GPU ordinal out of range. Should be within [" << 0 << ", " << nGpu - 1 << "]" << std::endl;
        return;
    }
    CUdevice cuDevice = 0;
    ck(cuDeviceGet(&cuDevice, iGpu));
    char szDeviceName[80];
    ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
    std::cout << "GPU in use: " << szDeviceName << std::endl;
    CUcontext cuContext = NULL;
    ck(cuCtxCreate(&cuContext, 0, cuDevice));

    std::unique_ptr<NvEncoderCudaIterative> pEnc(new NvEncoderCudaIterative(cuContext, nWidth, nHeight, eFormat));

	InitializeEncoder(pEnc, encodeCLIOptions, eFormat, nNumIterations);
	
	uint32_t nFrameBufferSize = pEnc->GetEncoderBufferCount();
	uint32_t nFrameSize = pEnc->GetFrameSize();
	uint32_t nDeviceFrameSize = pEnc->GetFrameSize(pEnc->GetCUDAPitch());

	NV_ENC_INITIALIZE_PARAMS initializeParams = pEnc->GetinitializeParams();
	NV_ENC_RECONFIGURE_PARAMS reconfigureParams;
	NV_ENC_CONFIG reInitCodecConfig;
	NV_ENC_RECONFIGURE_PARAMS reconfigureParamsOrg; // backup params
	memset(&reconfigureParamsOrg, 0, sizeof(reconfigureParamsOrg));
	reconfigureParamsOrg.version = NV_ENC_RECONFIGURE_PARAMS_VER;
	memcpy(&reconfigureParamsOrg.reInitEncodeParams, &initializeParams, sizeof(initializeParams));
	NV_ENC_CONFIG reInitCodecConfigOrg;
	memset(&reInitCodecConfigOrg, 0, sizeof(reInitCodecConfigOrg));
	reInitCodecConfigOrg.version = NV_ENC_CONFIG_VER;
	memcpy(&reInitCodecConfigOrg, initializeParams.encodeConfig, sizeof(reInitCodecConfigOrg));

    std::cout << std::endl;
    std::cout << "-- Running Iterative Encoder --" << std::endl;
    std::cout << "Target PSNR-Y range: [" << minTargetQuality << "dB, " << maxTargetQuality << "dB]" << std::endl;
	switch (initializeParams.encodeConfig->rcParams.rateControlMode)
	{
		case NV_ENC_PARAMS_RC_CONSTQP:
        {
			memcpy(&reconfigureParams, &reconfigureParamsOrg, sizeof(reconfigureParamsOrg));
			memcpy(&reInitCodecConfig, &reInitCodecConfigOrg, sizeof(reInitCodecConfigOrg));
			reconfigureParams.reInitEncodeParams.encodeConfig = &reInitCodecConfig;
            NV_ENC_QP QP;
			QP.qpIntra = initializeParams.encodeConfig->rcParams.constQP.qpIntra;
            if(!QP.qpIntra)
            {
                std::cout << "Warning: QP = 0. The QP should be higher than 0 for this sample. Otherwise an infinite PSNR is being targeted. Adjusting to QP = 20." << std::endl;
                QP.qpIntra = 20;
            }
            QP.qpInterP = QP.qpIntra;                                              
            QP.qpInterB = QP.qpIntra;
            reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.constQP = QP;
            std::cout << "Rate Control: Constant QP (QP = " << QP.qpIntra << ")" << std::endl;
            std::cout << "QP delta = " << nQPDelta << std::endl;
            reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;
			break;
        }
		case NV_ENC_PARAMS_RC_VBR:
        {
			memcpy(&reconfigureParams, &reconfigureParamsOrg, sizeof(reconfigureParamsOrg));
			memcpy(&reInitCodecConfig, &reInitCodecConfigOrg, sizeof(reInitCodecConfigOrg));
			reconfigureParams.reInitEncodeParams.encodeConfig = &reInitCodecConfig;
            uint8_t targetQuality = initializeParams.encodeConfig->rcParams.targetQuality;
			reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.targetQuality = targetQuality;
            std::cout << "Rate Control: VBR Constant Quality (init CQ = " << uint32_t(targetQuality) << ")" << std::endl;
            std::cout << "CQ delta = " << nQPDelta << std::endl;
            std::cout << "Maxbitrate = " << (float)(reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.maxBitRate)/1000000 << " Mbit/s" << std::endl;
			reconfigureParams.reInitEncodeParams.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR;
			break;
        }
		default:
            std::ostringstream err;
			err << "Error: This sample does not support CBR rate control." << std::endl;
            throw std::invalid_argument(err.str());
			break;
	}
    std::cout << std::endl;

    // Allocate host and device memory
	uint8_t* vHostFrame; // allocate single buffer for host (pinned memory)
	ck(cuMemAllocHost((void**)&vHostFrame, nFrameSize));    
    std::vector<CUdeviceptr> vDeviceFrameBuffer(nFrameBufferSize);
    for (size_t i = 0; i < nFrameBufferSize; i++) 
		ck(cuMemAlloc(&vDeviceFrameBuffer[i], nDeviceFrameSize));

	int nFrame = 0;

    StopWatch processingTime;
	processingTime.Start();

	while (true)
	{
		// Load the next frame from disk
		std::streamsize nRead = fpIn.read(reinterpret_cast<char*>(vHostFrame), nFrameSize).gcount();
		// For receiving encoded packets
		std::vector<std::vector<uint8_t>> vPacket;
		if (nRead == nFrameSize)
		{
			const NvEncInputFrame* encoderInputFrame = pEnc->GetNextInputFrame(nFrame); // Copy frame to device
			NvEncoderCuda::CopyToDeviceFrame(cuContext, vHostFrame, 0, (CUdeviceptr)encoderInputFrame->inputPtr,
				(int)encoderInputFrame->pitch,
				pEnc->GetEncodeWidth(),
				pEnc->GetEncodeHeight(),
				CU_MEMORYTYPE_HOST,
				encoderInputFrame->bufferFormat,
				encoderInputFrame->chromaOffsets,
				encoderInputFrame->numChromaPlanes);
            // Create backup frame buffer on device for metric calculation
            NvEncoderCuda::CopyToDeviceFrame(cuContext, encoderInputFrame->inputPtr, encoderInputFrame->pitch, (CUdeviceptr)vDeviceFrameBuffer[nFrame % nFrameBufferSize],
				(int)encoderInputFrame->pitch,
				pEnc->GetEncodeWidth(),
				pEnc->GetEncodeHeight(),
				CU_MEMORYTYPE_DEVICE,
				encoderInputFrame->bufferFormat,
				encoderInputFrame->chromaOffsets,
				encoderInputFrame->numChromaPlanes);
            pEnc->EncodeFrameConstantQuality(vPacket, vDeviceFrameBuffer, &reconfigureParams, minTargetQuality, maxTargetQuality, nQPDelta, nFrame);
		}
		else
		{
			pEnc->EndEncode(vPacket, vDeviceFrameBuffer, &reconfigureParams, minTargetQuality, maxTargetQuality, nQPDelta, nFrame);
		}
		nFrame++;
		for (std::vector<uint8_t> &packet : vPacket)
		{
			// For each encoded packet
			fpOut.write(reinterpret_cast<char*>(packet.data()), packet.size());
		}

		if (nRead != nFrameSize) break;
	}

    double pT = processingTime.Stop();
	std::cout << "Processing time = " << pT << " seconds, FPS=" << (nFrame - 1) / pT << " (#frames=" << (nFrame - 1) << ")" << std::endl;

    // Free host and device memory
	cuMemFreeHost(vHostFrame);
    for (size_t i = 0; i < nFrameBufferSize; i++)
		cuMemFree(vDeviceFrameBuffer[i]);

	pEnc->DestroyEncoder();

	std::cout << "Total frames encoded: " << nFrame - 1 << std::endl;

    fpOut.close();
    fpIn.close();

}

int main(int argc, char **argv)
{
    char szInFilePath[256] = "",
        szOutFilePath[256] = "";
    int nWidth = 0, nHeight = 0;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    int iGpu = 0;
    uint32_t nMaxNumIterations = 3;
    uint32_t nMinTargetQuality = 35;
    uint32_t nMaxTargetQuality = 40;
    uint32_t nQPDelta = 1;
    try
    {
        NvEncoderInitParam encodeCLIOptions;
        ParseCommandLine(argc, argv, szInFilePath, nWidth, nHeight, eFormat, szOutFilePath, encodeCLIOptions, iGpu, nMaxNumIterations, nMinTargetQuality, nMaxTargetQuality, nQPDelta);

        CheckInputFile(szInFilePath);
        ValidateResolution(nWidth, nHeight);
        ValidateQualityRange(nMinTargetQuality, nMaxTargetQuality);

        EncQual(szInFilePath, szOutFilePath, nWidth, nHeight, eFormat, iGpu, encodeCLIOptions, nMaxNumIterations, nMinTargetQuality, nMaxTargetQuality, nQPDelta);
    }
    catch (const std::exception &e)
    {
        std::cout << e.what();
    }
    return 0;
}
