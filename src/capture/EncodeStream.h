#pragma once
#include "AsyncWorker.h"
#include "CaptureTypes.h"
#include "OmniEnums.h"
#include "SessionHandler.h"
#include "nvenc.h"
#include <concepts>

template <typename T>
concept CapSource = requires {
    T::FrameAqMode;
    T::Type;
};

template <CapSource CaptureSource, typename EncoderType = NvencSession> struct EncodeStream
{
    CaptureSource* Source = nullptr;
    EncoderType* Encoder = nullptr;
    OmniNetSession<OmniMTU>* Target = nullptr;
    DeviceMap TargetDevice;
    AsyncWorker::Uncached Worker;

    static void
    CaptureSend(OmniNetSession<OmniMTU>* OmniNet, CaptureSource* Source, EncoderType* OmniEncode)
    {
        if constexpr (requires { Source->AcquireFrame(); }) {
            if (!Source->AcquireFrame())
                return;
        }

        OmniEncode->Encode();
        OmniNet->ChunkedSend(
            reinterpret_cast<char*>(OmniEncode->NVBitstreamLock.bitstreamBufferPtr),
            OmniEncode->NVBitstreamLock.bitstreamSizeInBytes,
            [OmniEncode]() { OmniEncode->NVUnlockBitStream(); }
        );
    }

    void Start(
        CaptureSource* Source_,
        EncoderType* Encoder_,
        OmniNetSession<OmniMTU>* Target_,
        DeviceMap TargetDevice_,
        const StreamConfig& Config = {}
    )
    {
        Source = Source_;
        Encoder = Encoder_;
        Target = Target_;
        TargetDevice = TargetDevice_;

        if constexpr (CaptureSource::Type == CaptureAPI::WGC) {
            Source->CreateMonitorCapSession(
                Config.Width, Config.Height, [OmniEncode = Encoder, OmniNet = Target](auto* Tex2D) {
                    if constexpr (requires { OmniEncode->ResolveCachedResource(Tex2D); }) {
                        OmniEncode->ResolveCachedResource(Tex2D);
                    }
                    OmniEncode->Encode();
                    if constexpr (requires { Tex2D->Release(); }) {
                        Tex2D->Release();
                    }
                    OmniNet->ChunkedSend(
                        reinterpret_cast<char*>(OmniEncode->NVBitstreamLock.bitstreamBufferPtr),
                        OmniEncode->NVBitstreamLock.bitstreamSizeInBytes,
                        [OmniEncode]() { OmniEncode->NVUnlockBitStream(); }
                    );
                }
            );
            Source->StartSession();
        } else if constexpr (CaptureSource::Type == CaptureAPI::PipeWire) {
            Source->SetupCapturePipeline(
                Config.Width, Config.Height, [OmniEncode = Encoder, OmniNet = Target](auto* frame) {
                    OmniEncode->Encode();
                    OmniNet->ChunkedSend(
                        reinterpret_cast<char*>(OmniEncode->NVBitstreamLock.bitstreamBufferPtr),
                        OmniEncode->NVBitstreamLock.bitstreamSizeInBytes,
                        [OmniEncode]() { OmniEncode->NVUnlockBitStream(); }
                    );
                }
            );
            Source->StartSession();
        } else if constexpr (CaptureSource::Type == CaptureAPI::DXGI) {
            Worker.StartSpinThread(
                [](OmniNetSession<OmniMTU>* OmniNet,
                   CaptureSource* Source,
                   EncoderType* OmniEncode) { CaptureSend(OmniNet, Source, OmniEncode); },
                Target,
                Source,
                Encoder
            );
        }
    }

    void Stop()
    {
        if constexpr (CaptureSource::FrameAqMode == FrameAquisition::EventDriven) {
            Source->CloseSession();
        } else {
            Worker.EndLoopedThread();
        }
    }
};
