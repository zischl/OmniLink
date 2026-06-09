#pragma once
#include "AsyncWorker.h"
#include "OmniEnums.h"
#include "SessionHandler.h"
#include "nvenc.h"
#include <concepts>

template <typename SourceType>
concept CapSource = requires(SourceType CaptureSource) {
    { CaptureSource.AcquireFrame() } -> std::same_as<void>;
    { CaptureSource.ReleaseFrame() } -> std::same_as<void>;
};

template <CapSource CaptureSource> struct EncodeStream
{
    CaptureSource* Source = nullptr;
    NvencSession* Encoder = nullptr;
    session* Target = nullptr;
    DeviceMap TargetDevice;
    AsyncWorker::Uncached Worker;

    static void CaptureSend(session* NetSession, CaptureSource* Source, NvencSession* NvEncode)
    {
        Source->AcquireFrame();
        NvEncode->Encode();
        NetSession->ChunkedSend(
            reinterpret_cast<char*>(NvEncode->NVBitstreamLock.bitstreamBufferPtr),
            NvEncode->NVBitstreamLock.bitstreamSizeInBytes);
        NvEncode->NVUnlockBitStream();
        Source->ReleaseFrame();
    }

    void
    Start(CaptureSource* Source, NvencSession* Encoder, session* Target, DeviceMap TargetDevice)
    {
        Source = Source;
        Encoder = Encoder;
        Target = Target;
        TargetDevice = TargetDevice;
        Worker.StartSpinThread(CaptureSend, Target, Source, Encoder);
    }

    void Stop() { Worker.EndLoopedThread(); }
};
