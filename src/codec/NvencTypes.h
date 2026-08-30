#ifndef NVENC_TYPES
#define NVENC_TYPES

#pragma once
#include <nvEncodeAPI.h>

struct NVec2U
{
    size_t Width = 1920;
    size_t Height = 1080;
};

struct NvencResourceRegConfig
{
    NVec2U Dimensions = {};
    NV_ENC_BUFFER_FORMAT NvencBufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
};

struct NvencStaticConfig : NvencResourceRegConfig
{
    GUID NvencCodecGUID = NV_ENC_CODEC_H264_GUID;
    GUID NvencProfileGUID = NV_ENC_CODEC_H264_GUID;
    NV_ENC_TUNING_INFO NvencTuningInfo = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
};

struct NvencStaticConfigEx : NvencStaticConfig
{
    int maxReferenceFrames;
    bool enableBFrames;
    bool enableSlicingMode;
};

struct NvencLiveConfig
{
    GUID NvencPresetGUID = NV_ENC_PRESET_P1_GUID;
    int avgBitrate;
    int maxBitrate;
};

struct NvencLiveConfigEx : NvencLiveConfig
{
    uint32_t FrameRateNum = 60;
    uint32_t FrameRateDen = 1;
    uint32_t EnablePTD = 1;
};

struct NvencH264Config
{
    uint32_t idrPeriod = NVENC_INFINITE_GOPLENGTH;
    uint32_t repeatSPSPPS = 1;
    uint32_t disableDeblockingFilterIDC = 0;

    uint32_t enableIntraRefresh = 0;
    uint32_t intraRefreshPeriod = 0;
    uint32_t intraRefreshCnt = 0;

    uint32_t level = NV_ENC_LEVEL_AUTOSELECT;
    uint32_t maxNumRefFrames = 1;
    NV_ENC_H264_ENTROPY_CODING_MODE entropyCodingMode = NV_ENC_H264_ENTROPY_CODING_MODE_CABAC;
    uint32_t sliceMode = 0;
};

struct NvencRCParams
{
    NV_ENC_PARAMS_RC_MODE rateControlMode = NV_ENC_PARAMS_RC_CBR;

    uint32_t averageBitRate = 12000000;
    uint32_t maxBitRate = 15000000;
    uint32_t vbvBufferSize = 0;
    uint32_t vbvInitialDelay = 0;

    uint16_t enableLookahead = 0;
    uint32_t lookaheadDepth = 0;
    uint32_t disableIadapt = 1;
    uint32_t disableBadapt = 1;

    uint32_t enableAQ = 0;
    uint32_t aqStrength = 0;
};

struct NvencPresetConfig
{
    uint32_t gopLength = NVENC_INFINITE_GOPLENGTH;
    int frameIntervalP = 1;
    NvencRCParams rcParams{};
    NvencH264Config h264Config{};
};

struct NvencInitConfig : NvencLiveConfigEx, NvencStaticConfig, NvencPresetConfig
{
};

#endif
