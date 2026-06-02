#ifndef OmniCapController_H
#define OmniCapController_H

#pragma once
#include "AsyncWorker.h"
#include "OmniEnums.h"
#include "SessionHandler.h"
#include "WinCap.h"
#include "nvenc.h"

enum CaptureMode { DXGI, WGC };

struct EncodeStream
{
    WGScreenCapture* WGSCapture = nullptr;
    DXGICapture* DXGISCapture = nullptr;

    ID3D11Texture2D* CaptureBuffer = nullptr;

    NvencSession* Encoder = nullptr;

    session* Target = nullptr;
    DeviceMap TargetDevice;

    AsyncWorker::Uncached Worker;
    bool Status = false;

    void Start(ID3D11Device* D3D11Device,
               ID3D11DeviceContext* D3D11Context,
               session* NetSession,
               DeviceMap TargetID,
               CaptureMode Mode,
               NVENCODER* NvEncodeAPI);

    void Stop();

    inline static void
    WGCapSend(session* NetSession, WGScreenCapture* WGSCapture, NvencSession* NvSession)
    {
        WGSCapture->WriteStateLock();

        NvSession->Encode();

        WGSCapture->WriteStateUnlock();

        // OutputDebugStringA((std::to_string(Nvs->NVBitstreamLock.bitstreamSizeInBytes)
        // + "\n").c_str());

        NetSession->ChunkedSend(
            reinterpret_cast<char*>(NvSession->NVBitstreamLock.bitstreamBufferPtr),
            NvSession->NVBitstreamLock.bitstreamSizeInBytes);

        NvSession->NVUnlockBitStream();
    }

    inline static void
    DXGICapSend(session* NetSession, DXGICapture* DXGICap, NvencSession* NvSession)
    {
        if (DXGICap->CaptureDXGI() == 0) {
            NvSession->Encode();

            // OutputDebugStringA((std::to_string(Nvs->NVBitstreamLock.bitstreamSizeInBytes)
            // + "\n").c_str());

            // NvencOutputTest(Nvs->NVBitstreamLock, index+"hellow there");

            NetSession->ChunkedSend(
                reinterpret_cast<char*>(NvSession->NVBitstreamLock.bitstreamBufferPtr),
                NvSession->NVBitstreamLock.bitstreamSizeInBytes);

            NvSession->NVUnlockBitStream();
        }
    }
};

struct CaptureController
{
    std::unordered_map<uint8_t, EncodeStream> Streams;
    uint8_t StreamCount = 0;
    NVENCODER NvEncodeAPI;

    EncodeStream* AddStream();
    void RemoveStream(size_t StreamID);
    void StopAll();
};

#endif
