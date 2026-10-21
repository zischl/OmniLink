#pragma once
#include "AsyncWorker.hpp"
#include "CaptureTypes.hpp"
#include "OmniEnums.hpp"
#include "OmniLogger.hpp"
#include "SessionHandler.hpp"
#include "nvenc.hpp"
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
    typename EncoderType                = NvencSession,
    OmniNetStreamerType OmniNetStreamer = OmniNetSubStream>
struct EncodeStream
{
    CaptureSource*           Source  = nullptr;
    EncoderType*             Encoder = nullptr;
    OmniNetStreamer*         Target  = nullptr;
    DeviceMap                TargetDevice;
    AsyncWorker::Uncached    Worker;
    std::atomic<StreamState> State{StreamState::Stopped};

    void RequestKeyframe()
    {
        if (Encoder) {
            Encoder->RequestKeyframe();
        }
    }

    void SetStreamState(StreamState NewState)
    {
        State.store(NewState, std::memory_order_release);
        if (NewState == StreamState::Active && Encoder) {
            Encoder->RequestKeyframe();
        }
    }

    void SetStreamingState(bool Active)
    {
        SetStreamState(Active ? StreamState::Active : StreamState::Paused);
    }

    StreamState GetStreamState() const { return State.load(std::memory_order_acquire); }
    bool        GetStreamingState() const
    {
        return State.load(std::memory_order_acquire) == StreamState::Active;
    }

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

    static void CaptureSend(
        OmniNetStreamer*          OmniNet,
        CaptureSource*            Source,
        EncoderType*              OmniEncode,
        std::atomic<StreamState>* StateFlag
    )
    {
        if constexpr (requires { Source->AcquireFrame(); }) {
            if (!Source->AcquireFrame())
                return;
        }

        if (StateFlag->load(std::memory_order_relaxed) != StreamState::Active)
            return;

        Zencode(OmniNet, OmniEncode);
    }

    void Start(
        CaptureSource*      Source_,
        EncoderType*        Encoder_,
        OmniNetStreamer*    Target_,
        DeviceMap           TargetDevice_,
        const StreamConfig& Config = {}
    )
    {
        Source       = Source_;
        Encoder      = Encoder_;
        Target       = Target_;
        TargetDevice = TargetDevice_;
        State.store(StreamState::Ready, std::memory_order_release);

        if constexpr (CaptureSource::Type == CaptureAPI::WGC) {
            auto FrameCallback = [this](auto* Tex2D) {
                const StreamState CurrentState = State.load(std::memory_order_relaxed);
                if (CurrentState != StreamState::Active) [[unlikely]] {
                    if constexpr (requires { Tex2D->Release(); }) {
                        Tex2D->Release();
                    }
                    return;
                }
                if constexpr (requires { Encoder->ResolveCachedResource(Tex2D); }) {
                    Encoder->ResolveCachedResource(Tex2D);
                }
                Zencode(Target, Encoder);
                if constexpr (requires { Tex2D->Release(); }) {
                    Tex2D->Release();
                }
            };
#if defined(_WIN32)
            if (Config.WindowHandle != NULL) {
                if constexpr (requires {
                                  Source->CreateWindowCapSession(
                                      Config.WindowHandle,
                                      Config.Width,
                                      Config.Height,
                                      FrameCallback
                                  );
                              }) {
                    Source->CreateWindowCapSession(
                        Config.WindowHandle, Config.Width, Config.Height, FrameCallback
                    );
                } else {
                    Source->CreateMonitorCapSession(Config.Width, Config.Height, FrameCallback);
                }
            } else {
                Source->CreateMonitorCapSession(Config.Width, Config.Height, FrameCallback);
            }
#else
            Source->CreateMonitorCapSession(Config.Width, Config.Height, frameCallback);
#endif
            Source->StartSession();
        } else if constexpr (CaptureSource::Type == CaptureAPI::PipeWire) {
            Source->SetupCapturePipeline(Config.Width, Config.Height, [this](auto* frame) {
                if (State.load(std::memory_order_relaxed) != StreamState::Active)
                    return;
                Zencode(Target, Encoder);
            });
            Source->StartSession();
        } else if constexpr (CaptureSource::Type == CaptureAPI::DXGI) {
            Worker.StartSpinThread(
                [](OmniNetStreamer*          OmniNet,
                   CaptureSource*            Source,
                   EncoderType*              OmniEncode,
                   std::atomic<StreamState>* StateFlag) {
                    CaptureSend(OmniNet, Source, OmniEncode, StateFlag);
                },
                Target,
                Source,
                Encoder,
                &State
            );
        }
    }

    void Stop()
    {
        State.store(StreamState::Stopped, std::memory_order_release);
        if constexpr (CaptureSource::FrameAqMode == FrameAquisition::EventDriven) {
            Source->CloseSession();
        } else {
            Worker.EndLoopedThread();
        }
    }
};
