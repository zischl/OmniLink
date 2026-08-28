#include "AudioRender.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

AudioRender::AudioRender()
{
    SamplesRingBuffer.resize(RingBufferCapacity, 0);
    RecvPoolBuffer.resize(RecvSlotCount * RecvSlotSize, 0);
}

AudioRender::~AudioRender()
{
    Stop();
}

bool AudioRender::Init(uint32_t InSampleRate, uint16_t InChannels)
{
    if (GetState() != AudioRenderState::Inactive)
        return false;
    TargetSampleRate = InSampleRate;
    TargetChannels = InChannels;
    return true;
}

bool AudioRender::Start()
{
    AudioRenderState Expected = AudioRenderState::Inactive;
    if (!RenderState.compare_exchange_strong(Expected, AudioRenderState::Active)) {
        return true;
    }
    StopRequestedState = false;
    WorkerThread = std::thread(&AudioRender::RenderWorkerThread, this);
    return true;
}

void AudioRender::Stop()
{
    StopRequestedState = true;
    RenderState.store(AudioRenderState::Inactive, std::memory_order_relaxed);
    if (WorkerThread.joinable()) {
        WorkerThread.join();
    }
}

void AudioRender::Pause()
{
    AudioRenderState Expected = AudioRenderState::Active;
    RenderState.compare_exchange_strong(Expected, AudioRenderState::Paused);
}

void AudioRender::Resume()
{
    AudioRenderState Expected = AudioRenderState::Paused;
    RenderState.compare_exchange_strong(Expected, AudioRenderState::Active);
}

void AudioRender::PushSample(const uint8_t* Data, size_t ByteCount)
{
    if (!Data || ByteCount == 0)
        return;

    std::lock_guard<std::mutex> Lock(RingBufferMutex);

    if (ByteCount > RingBufferCapacity) {
        Data += (ByteCount - RingBufferCapacity);
        ByteCount = RingBufferCapacity;
    }

    size_t PrimaryChunk = (std::min)(ByteCount, RingBufferCapacity - WriteByteIndex);
    size_t SecondaryChunk = ByteCount - PrimaryChunk;

    std::memcpy(&SamplesRingBuffer[WriteByteIndex], Data, PrimaryChunk);
    if (SecondaryChunk > 0) {
        std::memcpy(&SamplesRingBuffer[0], Data + PrimaryChunk, SecondaryChunk);
    }

    WriteByteIndex = (WriteByteIndex + ByteCount) & RingBufferMask;
    BufferedBytes += ByteCount;

    if (BufferedBytes > RingBufferCapacity) {
        BufferedBytes = RingBufferCapacity;
        ReadByteIndex = WriteByteIndex;
    }
}

size_t AudioRender::PopSample32To32(float* Buffer, size_t RequestedFrames, uint32_t DeviceChannels)
{
    if (!Buffer || RequestedFrames == 0 || DeviceChannels == 0)
        return 0;

    std::lock_guard<std::mutex> Lock(RingBufferMutex);
    uint32_t                    Channels = ActiveChannels.load(std::memory_order_relaxed);
    if (Channels == 0)
        Channels = 2;

    size_t FrameSizeBytes = Channels * sizeof(float);
    size_t FramesAvailable = BufferedBytes / FrameSizeBytes;
    size_t FramesToRead = (std::min)(RequestedFrames, FramesAvailable);

    if (FramesToRead > 0) {
        if (Channels == DeviceChannels) {
            size_t BytesToRead = FramesToRead * FrameSizeBytes;
            size_t FirstChunk = (std::min)(BytesToRead, RingBufferCapacity - ReadByteIndex);
            size_t SecondChunk = BytesToRead - FirstChunk;

            std::memcpy(Buffer, &SamplesRingBuffer[ReadByteIndex], FirstChunk);
            if (SecondChunk > 0) {
                std::memcpy(
                    reinterpret_cast<uint8_t*>(Buffer) + FirstChunk,
                    &SamplesRingBuffer[0],
                    SecondChunk
                );
            }
            ReadByteIndex = (ReadByteIndex + BytesToRead) & RingBufferMask;
            BufferedBytes -= BytesToRead;
        } else {
            for (size_t Frame = 0; Frame < FramesToRead; ++Frame) {
                float Left = *reinterpret_cast<const float*>(&SamplesRingBuffer[ReadByteIndex]);
                ReadByteIndex = (ReadByteIndex + sizeof(float)) & RingBufferMask;

                float Right = Left;
                if (Channels >= 2) {
                    Right = *reinterpret_cast<const float*>(&SamplesRingBuffer[ReadByteIndex]);
                    ReadByteIndex = (ReadByteIndex + sizeof(float)) & RingBufferMask;
                }

                if (DeviceChannels == 1) {
                    Buffer[Frame] = (Left + Right) * 0.5f;
                } else if (DeviceChannels >= 2) {
                    Buffer[Frame * DeviceChannels] = Left;
                    Buffer[Frame * DeviceChannels + 1] = Right;
                    for (uint32_t c = 2; c < DeviceChannels; ++c) {
                        Buffer[Frame * DeviceChannels + c] = 0.0f;
                    }
                }
            }
            BufferedBytes -= (FramesToRead * FrameSizeBytes);
        }
    }

    // Pad underrun with silence
    if (FramesToRead < RequestedFrames) {
        size_t SilenceStart = FramesToRead * DeviceChannels;
        size_t TotalSamples = RequestedFrames * DeviceChannels;
        std::fill(Buffer + SilenceStart, Buffer + TotalSamples, 0.0f);
    }

    return FramesToRead;
}

size_t AudioRender::PopSample16To32(float* Buffer, size_t RequestedFrames, uint32_t DeviceChannels)
{
    if (!Buffer || RequestedFrames == 0 || DeviceChannels == 0)
        return 0;

    std::lock_guard<std::mutex> Lock(RingBufferMutex);
    uint32_t                    Channels = ActiveChannels.load(std::memory_order_relaxed);
    if (Channels == 0)
        Channels = 2;

    size_t FrameSizeBytes = Channels * sizeof(int16_t);
    size_t FramesAvailable = BufferedBytes / FrameSizeBytes;
    size_t FramesToRead = (std::min)(RequestedFrames, FramesAvailable);

    constexpr float Scale = 1.0f / 32768.0f;

    for (size_t f = 0; f < FramesToRead; ++f) {
        int16_t LeftRaw = *reinterpret_cast<const int16_t*>(&SamplesRingBuffer[ReadByteIndex]);
        ReadByteIndex = (ReadByteIndex + sizeof(int16_t)) & RingBufferMask;
        float Left = static_cast<float>(LeftRaw) * Scale;

        float Right = Left;
        if (Channels >= 2) {
            int16_t RightRaw = *reinterpret_cast<const int16_t*>(&SamplesRingBuffer[ReadByteIndex]);
            ReadByteIndex = (ReadByteIndex + sizeof(int16_t)) & RingBufferMask;
            Right = static_cast<float>(RightRaw) * Scale;
        }

        if (DeviceChannels == 1) {
            Buffer[f] = (Left + Right) * 0.5f;
        } else if (DeviceChannels >= 2) {
            Buffer[f * DeviceChannels] = Left;
            Buffer[f * DeviceChannels + 1] = Right;
            for (uint32_t c = 2; c < DeviceChannels; ++c) {
                Buffer[f * DeviceChannels + c] = 0.0f;
            }
        }
    }

    BufferedBytes -= (FramesToRead * FrameSizeBytes);

    if (FramesToRead < RequestedFrames) {
        size_t SilenceStart = FramesToRead * DeviceChannels;
        size_t TotalSamples = RequestedFrames * DeviceChannels;
        std::fill(Buffer + SilenceStart, Buffer + TotalSamples, 0.0f);
    }

    return FramesToRead;
}

size_t
AudioRender::PopSample16To16(int16_t* Buffer, size_t RequestedFrames, uint32_t DeviceChannels)
{
    if (!Buffer || RequestedFrames == 0 || DeviceChannels == 0)
        return 0;

    std::lock_guard<std::mutex> Lock(RingBufferMutex);
    uint32_t                    Channels = ActiveChannels.load(std::memory_order_relaxed);
    if (Channels == 0)
        Channels = 2;

    size_t FrameSizeBytes = Channels * sizeof(int16_t);
    size_t FramesAvailable = BufferedBytes / FrameSizeBytes;
    size_t FramesToRead = (std::min)(RequestedFrames, FramesAvailable);

    if (FramesToRead > 0) {
        if (Channels == DeviceChannels) {
            size_t BytesToRead = FramesToRead * FrameSizeBytes;
            size_t FirstChunk = (std::min)(BytesToRead, RingBufferCapacity - ReadByteIndex);
            size_t SecondChunk = BytesToRead - FirstChunk;

            std::memcpy(Buffer, &SamplesRingBuffer[ReadByteIndex], FirstChunk);
            if (SecondChunk > 0) {
                std::memcpy(
                    reinterpret_cast<uint8_t*>(Buffer) + FirstChunk,
                    &SamplesRingBuffer[0],
                    SecondChunk
                );
            }
            ReadByteIndex = (ReadByteIndex + BytesToRead) & RingBufferMask;
            BufferedBytes -= BytesToRead;
        } else {
            for (size_t f = 0; f < FramesToRead; ++f) {
                int16_t Left = *reinterpret_cast<const int16_t*>(&SamplesRingBuffer[ReadByteIndex]);
                ReadByteIndex = (ReadByteIndex + sizeof(int16_t)) & RingBufferMask;

                int16_t Right = Left;
                if (Channels >= 2) {
                    Right = *reinterpret_cast<const int16_t*>(&SamplesRingBuffer[ReadByteIndex]);
                    ReadByteIndex = (ReadByteIndex + sizeof(int16_t)) & RingBufferMask;
                }

                if (DeviceChannels == 1) {
                    Buffer[f] = static_cast<int16_t>((static_cast<int32_t>(Left) + Right) / 2);
                } else if (DeviceChannels >= 2) {
                    Buffer[f * DeviceChannels] = Left;
                    Buffer[f * DeviceChannels + 1] = Right;
                    for (uint32_t c = 2; c < DeviceChannels; ++c) {
                        Buffer[f * DeviceChannels + c] = 0;
                    }
                }
            }
            BufferedBytes -= (FramesToRead * FrameSizeBytes);
        }
    }

    if (FramesToRead < RequestedFrames) {
        size_t SilenceStart = FramesToRead * DeviceChannels;
        size_t TotalSamples = RequestedFrames * DeviceChannels;
        std::fill(Buffer + SilenceStart, Buffer + TotalSamples, static_cast<int16_t>(0));
    }

    return FramesToRead;
}

size_t
AudioRender::PopSample32To16(int16_t* Buffer, size_t RequestedFrames, uint32_t DeviceChannels)
{
    if (!Buffer || RequestedFrames == 0 || DeviceChannels == 0)
        return 0;

    std::lock_guard<std::mutex> Lock(RingBufferMutex);
    uint32_t                    Channels = ActiveChannels.load(std::memory_order_relaxed);
    if (Channels == 0)
        Channels = 2;

    size_t FrameSizeBytes = Channels * sizeof(float);
    size_t FramesAvailable = BufferedBytes / FrameSizeBytes;
    size_t FramesToRead = (std::min)(RequestedFrames, FramesAvailable);

    for (size_t f = 0; f < FramesToRead; ++f) {
        float LeftRaw = *reinterpret_cast<const float*>(&SamplesRingBuffer[ReadByteIndex]);
        ReadByteIndex = (ReadByteIndex + sizeof(float)) & RingBufferMask;
        float Left = (std::max)(-1.0f, (std::min)(1.0f, LeftRaw));

        float Right = Left;
        if (Channels >= 2) {
            float RightRaw = *reinterpret_cast<const float*>(&SamplesRingBuffer[ReadByteIndex]);
            ReadByteIndex = (ReadByteIndex + sizeof(float)) & RingBufferMask;
            Right = (std::max)(-1.0f, (std::min)(1.0f, RightRaw));
        }

        if (DeviceChannels == 1) {
            float Mono = (Left + Right) * 0.5f;
            Buffer[f] = static_cast<int16_t>(Mono * 32767.0f);
        } else if (DeviceChannels >= 2) {
            Buffer[f * DeviceChannels] = static_cast<int16_t>(Left * 32767.0f);
            Buffer[f * DeviceChannels + 1] = static_cast<int16_t>(Right * 32767.0f);
            for (uint32_t c = 2; c < DeviceChannels; ++c) {
                Buffer[f * DeviceChannels + c] = 0;
            }
        }
    }

    BufferedBytes -= (FramesToRead * FrameSizeBytes);

    if (FramesToRead < RequestedFrames) {
        size_t SilenceStart = FramesToRead * DeviceChannels;
        size_t TotalSamples = RequestedFrames * DeviceChannels;
        std::fill(Buffer + SilenceStart, Buffer + TotalSamples, static_cast<int16_t>(0));
    }

    return FramesToRead;
}

void AudioRender::WritePacket(const uint8_t* PacketData, size_t PacketSize)
{
    if (!PacketData || PacketSize < sizeof(AudioFrameHeader))
        return;

    const AudioFrameHeader* Header = reinterpret_cast<const AudioFrameHeader*>(PacketData);
    if (Header->Liss != 0x4F4D4E49)
        return;

    if (PacketSize < sizeof(AudioFrameHeader) + Header->PayloadSize)
        return;

    ActiveCodec.store(Header->AudioCodecType, std::memory_order_relaxed);
    ActiveChannels.store(Header->Channels, std::memory_order_relaxed);
    ActiveSampleRate.store(Header->SampleRate, std::memory_order_relaxed);
    ActiveBitsPerSample.store(Header->BitsPerSample, std::memory_order_relaxed);

    const uint8_t* Payload = PacketData + sizeof(AudioFrameHeader);
    PushSample(Payload, Header->PayloadSize);
}

void AudioRender::GetBufferPool(
    char*&    OutData,
    uint32_t& OutDataSize,
    uint32_t& OutSlotCount,
    void (**OutOnSlotComplete)(void*, uint32_t, uint32_t),
    void*& OutCtx
)
{
    OutData = reinterpret_cast<char*>(RecvPoolBuffer.data());
    OutDataSize = static_cast<uint32_t>(RecvPoolBuffer.size());
    OutSlotCount = RecvSlotCount;
    *OutOnSlotComplete = [](void* Ctx, uint32_t Slot, uint32_t Size) {
        auto* Renderer = reinterpret_cast<AudioRender*>(Ctx);
        if (Renderer && Slot < RecvSlotCount) {
            const uint8_t* SlotData = Renderer->RecvPoolBuffer.data() + (Slot * RecvSlotSize);
            Renderer->WritePacket(SlotData, Size);
        }
    };
    OutCtx = this;
}

void AudioRender::RenderWorkerThread()
{
    HRESULT HResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool    CoInitialized = SUCCEEDED(HResult);

    WAVEFORMATEX* WVFormat = nullptr;
    HANDLE        hEvent = nullptr;

    struct ScopeExit
    {
        std::function<void()> ExitFn;
        ~ScopeExit()
        {
            if (ExitFn)
                ExitFn();
        }
    } CleanupGuard{[&]() {
        if (WVFormat)
            CoTaskMemFree(WVFormat);
        if (hEvent)
            CloseHandle(hEvent);
        if (CoInitialized)
            CoUninitialize();
    }};

    ComPtr<IMMDeviceEnumerator> Enumerator;
    ComPtr<IMMDevice>           Device;
    ComPtr<IAudioClient>        AudioClient;
    ComPtr<IAudioClient3>       AudioClient3;
    ComPtr<IAudioRenderClient>  RenderClient;

    HResult = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&Enumerator
    );
    if (FAILED(HResult))
        return;

    HResult = Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &Device);
    if (FAILED(HResult))
        return;

    HResult = Device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)&AudioClient3);
    if (SUCCEEDED(HResult)) {
        AudioClient = AudioClient3;
    } else {
        HResult = Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&AudioClient);
        if (FAILED(HResult))
            return;
    }

    HResult = AudioClient->GetMixFormat(&WVFormat);
    if (FAILED(HResult))
        return;

    DWORD StreamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    bool  InitializedWithClient3 = false;

    if (AudioClient3) {
        UINT32  defaultPeriod = 0, fundamentalPeriod = 0, minPeriod = 0, maxPeriod = 0;
        HRESULT hrPeriod = AudioClient3->GetSharedModeEnginePeriod(
            WVFormat, &defaultPeriod, &fundamentalPeriod, &minPeriod, &maxPeriod
        );
        if (SUCCEEDED(hrPeriod) && minPeriod > 0) {
            HResult =
                AudioClient3->InitializeSharedAudioStream(StreamFlags, minPeriod, WVFormat, NULL);
            if (SUCCEEDED(HResult)) {
                InitializedWithClient3 = true;
            }
        }
    }

    if (!InitializedWithClient3) {
        REFERENCE_TIME RequestedDuration = REFTIMES_PER_MILLISEC * 20;
        HResult = AudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED, StreamFlags, RequestedDuration, 0, WVFormat, NULL
        );
        if (FAILED(HResult))
            return;
    }

    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hEvent)
        return;

    HResult = AudioClient->SetEventHandle(hEvent);
    if (FAILED(HResult))
        return;

    HResult = AudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&RenderClient);
    if (FAILED(HResult))
        return;

    UINT32 BufferFrameCount = 0;
    HResult = AudioClient->GetBufferSize(&BufferFrameCount);
    if (FAILED(HResult))
        return;

    BYTE* InitialData = nullptr;
    HResult = RenderClient->GetBuffer(BufferFrameCount, &InitialData);
    if (SUCCEEDED(HResult)) {
        std::memset(InitialData, 0, BufferFrameCount * WVFormat->nBlockAlign);
        RenderClient->ReleaseBuffer(BufferFrameCount, 0);
    }

    HResult = AudioClient->Start();
    if (FAILED(HResult))
        return;

    bool WVFormatFloat = false;
    if (WVFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        WVFormatFloat = true;
    } else if (WVFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* WVFormatEx =
            reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(WVFormat);
        if (WVFormatEx->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            WVFormatFloat = true;
        }
    }

    while (!StopRequestedState.load()) {
        DWORD WaitResult = WaitForSingleObject(hEvent, 100);
        if (StopRequestedState.load())
            break;

        if (WaitResult == WAIT_OBJECT_0) {
            UINT32 PaddingFrames = 0;
            HResult = AudioClient->GetCurrentPadding(&PaddingFrames);
            if (FAILED(HResult))
                continue;

            UINT32 FramesAvailable =
                (BufferFrameCount > PaddingFrames) ? (BufferFrameCount - PaddingFrames) : 0;
            if (FramesAvailable == 0)
                continue;

            BYTE* DataPtr = nullptr;
            HResult = RenderClient->GetBuffer(FramesAvailable, &DataPtr);
            if (FAILED(HResult))
                continue;

            if (GetState() == AudioRenderState::Paused) {
                std::memset(DataPtr, 0, FramesAvailable * WVFormat->nBlockAlign);
            } else {
                CodecType IncomingCodec = ActiveCodec.load(std::memory_order_relaxed);

                if (WVFormatFloat && WVFormat->wBitsPerSample == 32) {
                    if (IncomingCodec == CodecType::PCM_FLOAT32) {
                        PopSample32To32(
                            reinterpret_cast<float*>(DataPtr), FramesAvailable, WVFormat->nChannels
                        );
                    } else {
                        PopSample16To32(
                            reinterpret_cast<float*>(DataPtr), FramesAvailable, WVFormat->nChannels
                        );
                    }
                } else if (!WVFormatFloat && WVFormat->wBitsPerSample == 16) {
                    if (IncomingCodec == CodecType::PCM_S16LE) {
                        PopSample16To16(
                            reinterpret_cast<int16_t*>(DataPtr),
                            FramesAvailable,
                            WVFormat->nChannels
                        );
                    } else {
                        PopSample32To16(
                            reinterpret_cast<int16_t*>(DataPtr),
                            FramesAvailable,
                            WVFormat->nChannels
                        );
                    }
                } else {
                    std::memset(DataPtr, 0, FramesAvailable * WVFormat->nBlockAlign);
                }
            }

            RenderClient->ReleaseBuffer(FramesAvailable, 0);
        }
    }

    AudioClient->Stop();
}
