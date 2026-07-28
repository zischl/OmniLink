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

template <typename T>
concept OmniNetStreamerType = requires(T* OmniNetType, CHAR* Data, int Size) {
    { OmniNetType->ChunkedSend(Data, Size) } -> std::same_as<bool>;
};

template <
    CapSource CaptureSource,
    typename EncoderType = NvencSession,
    OmniNetStreamerType OmniNetStreamer = OmniNetSubStream>
struct EncodeStream
{
    CaptureSource* Source = nullptr;
    EncoderType* Encoder = nullptr;
    OmniNetStreamer* Target = nullptr;
    DeviceMap TargetDevice;
    AsyncWorker::Uncached Worker;

    // U might be wondering what this is... it's just Encode and Send
    inline static void Zencode(OmniNetStreamer* OmniNet, EncoderType* OmniEncode)
    {
        bool CaptureSendState = false;
        if constexpr (requires {
                          { OmniEncode->Encode() } -> std::same_as<bool>;
                      }) {
            CaptureSendState = OmniEncode->Encode();
        } else {
            OmniEncode->Encode();
            CaptureSendState = true;
        }

        if (CaptureSendState) {
            if constexpr (requires { OmniEncode->NVBitstreamLocks; }) {
                size_t SlotIndex = OmniEncode->GetLastEncodedSlotIndex();
                OmniNet->ChunkedSend(
                    reinterpret_cast<char*>(OmniEncode->NVBitstreamLock.bitstreamBufferPtr),
                    OmniEncode->NVBitstreamLock.bitstreamSizeInBytes,
                    [](void* encoder, size_t slot) {
                        reinterpret_cast<EncoderType*>(encoder)->ReleaseBuffer(slot);
                    },
                    OmniEncode,
                    SlotIndex
                );
            } else {
                OmniNet->ChunkedSend(
                    reinterpret_cast<char*>(OmniEncode->NVBitstreamLock.bitstreamBufferPtr),
                    OmniEncode->NVBitstreamLock.bitstreamSizeInBytes,
                    [](void* encoder, size_t) {
                        reinterpret_cast<EncoderType*>(encoder)->NVUnlockBitStream();
                    },
                    OmniEncode
                );
            }
        }
    }

    static void
    CaptureSend(OmniNetStreamer* OmniNet, CaptureSource* Source, EncoderType* OmniEncode)
    {
        if constexpr (requires { Source->AcquireFrame(); }) {
            if (!Source->AcquireFrame())
                return;
        }

        Zencode(OmniNet, OmniEncode);
    }

    void Start(
        CaptureSource* Source_,
        EncoderType* Encoder_,
        OmniNetStreamer* Target_,
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
                    Zencode(OmniNet, OmniEncode);
                    if constexpr (requires { Tex2D->Release(); }) {
                        Tex2D->Release();
                    }
                }
            );
            Source->StartSession();
        } else if constexpr (CaptureSource::Type == CaptureAPI::PipeWire) {
            Source->SetupCapturePipeline(
                Config.Width, Config.Height, [OmniEncode = Encoder, OmniNet = Target](auto* frame) {
                    Zencode(OmniNet, OmniEncode);
                }
            );
            Source->StartSession();
        } else if constexpr (CaptureSource::Type == CaptureAPI::DXGI) {
            Worker.StartSpinThread(
                [](OmniNetStreamer* OmniNet, CaptureSource* Source, EncoderType* OmniEncode) {
                    CaptureSend(OmniNet, Source, OmniEncode);
                },
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
