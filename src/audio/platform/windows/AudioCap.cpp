#include "AudioCap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

// AudioCapture
AudioCapture::AudioCapture() = default;

AudioCapture::~AudioCapture()
{
    Stop();
}

bool AudioCapture::Init(AudioDeviceType InDeviceType, uint32_t InSampleRate, uint16_t InChannels)
{
    if (GetState() != CaptureState::Inactive)
        return false;
    DeviceType = InDeviceType;
    TargetSampleRate = InSampleRate;
    TargetChannels = InChannels;
    return true;
}

void AudioCapture::SetPacketCallback(AudioPacketCallback InCallback)
{
    Callback = std::move(InCallback);
}

bool AudioCapture::Start()
{
    CaptureState Expected = CaptureState::Inactive;
    if (!AudioCapState.compare_exchange_strong(Expected, CaptureState::Active)) {
        return true;
    }
    StopRequestedState = false;
    WorkerThread = std::thread(&AudioCapture::CaptureWorkerThread, this);
    return true;
}

void AudioCapture::Stop()
{
    StopRequestedState = true;
    AudioCapState.store(CaptureState::Inactive, std::memory_order_relaxed);
    if (WorkerThread.joinable()) {
        WorkerThread.join();
    }
}

void AudioCapture::Pause()
{
    CaptureState Expected = CaptureState::Active;
    AudioCapState.compare_exchange_strong(Expected, CaptureState::Paused);
}

void AudioCapture::Resume()
{
    CaptureState Expected = CaptureState::Paused;
    AudioCapState.compare_exchange_strong(Expected, CaptureState::Active);
}

void AudioCapture::CaptureWorkerThread()
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
    ComPtr<IAudioCaptureClient> CaptureClient;

    EDataFlow DataFlow = (DeviceType == AudioDeviceType::Microphone) ? eCapture : eRender;
    DWORD     StreamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    if (DeviceType == AudioDeviceType::SystemLoopback) {
        StreamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    }
    bool InitializedWithClient3 = false;

    HResult = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**)&Enumerator
    );
    if (FAILED(HResult))
        return;

    HResult = Enumerator->GetDefaultAudioEndpoint(DataFlow, eConsole, &Device);
    if (FAILED(HResult))
        return;

    // Attempt IAudioClient3
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
        // Fallback to standard WASAPI shared event driven mode
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

    HResult = AudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&CaptureClient);
    if (FAILED(HResult))
        return;

    HResult = AudioClient->Start();
    if (FAILED(HResult))
        return;

    while (!StopRequestedState.load()) {
        DWORD WaitResult = WaitForSingleObject(hEvent, 100);
        if (StopRequestedState.load())
            break;

        if (WaitResult == WAIT_OBJECT_0) {
            UINT32 PacketLength = 0;
            HResult = CaptureClient->GetNextPacketSize(&PacketLength);
            while (SUCCEEDED(HResult) && PacketLength > 0 && !StopRequestedState.load()) {
                BYTE*  Data = nullptr;
                UINT32 NumFramesAvailable = 0;
                DWORD  Flags = 0;

                HResult = CaptureClient->GetBuffer(&Data, &NumFramesAvailable, &Flags, NULL, NULL);
                if (FAILED(HResult))
                    break;

                if (GetState() != CaptureState::Paused) {
                    if (Flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        ProcessAudioPacket(nullptr, NumFramesAvailable, WVFormat);
                    } else {
                        ProcessAudioPacket(Data, NumFramesAvailable, WVFormat);
                    }
                }

                CaptureClient->ReleaseBuffer(NumFramesAvailable);
                HResult = CaptureClient->GetNextPacketSize(&PacketLength);
            }
        }
    }

    AudioClient->Stop();
}

// Supporting both 16 bit stereo and 32 bit float outputs
void AudioCapture::ProcessAudioPacket(
    const uint8_t* InputData, uint32_t NumFrames, const WAVEFORMATEX* WVFormat
)
{
    if (NumFrames == 0 || !WVFormat)
        return;

    WORD  Channels = WVFormat->nChannels;
    WORD  BitsPerSample = WVFormat->wBitsPerSample;
    DWORD SampleRate = WVFormat->nSamplesPerSec;

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

    uint32_t OutputChannels = TargetChannels;
    size_t   TotalOutSamples = static_cast<size_t>(NumFrames) * OutputChannels;
    size_t   PcmPayloadSize = S16NormalizerState ? (TotalOutSamples * sizeof(int16_t))
                                                 : (TotalOutSamples * sizeof(float));

    if (PcmConversionBuffer.size() < PcmPayloadSize) {
        PcmConversionBuffer.resize(PcmPayloadSize);
    }

    if (S16NormalizerState) {
        int16_t* OutputSamples = reinterpret_cast<int16_t*>(PcmConversionBuffer.data());

        if (InputData == nullptr) {
            std::fill(OutputSamples, OutputSamples + TotalOutSamples, static_cast<int16_t>(0));
        } else if (WVFormatFloat && BitsPerSample == 32) {
            const float* FloatPCM = reinterpret_cast<const float*>(InputData);
            for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                float Left = 0.0f, Right = 0.0f;
                if (Channels == 1) {
                    Left = Right = FloatPCM[Frame];
                } else if (Channels >= 2) {
                    Left = FloatPCM[Frame * Channels];
                    Right = FloatPCM[Frame * Channels + 1];
                }
                Left = (std::max)(-1.0f, (std::min)(1.0f, Left));
                Right = (std::max)(-1.0f, (std::min)(1.0f, Right));

                OutputSamples[Frame * 2] = static_cast<int16_t>(Left * 32767.0f);
                OutputSamples[Frame * 2 + 1] = static_cast<int16_t>(Right * 32767.0f);
            }
        } else if (!WVFormatFloat && BitsPerSample == 16) {
            const int16_t* Int16PCM = reinterpret_cast<const int16_t*>(InputData);
            for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                if (Channels == 1) {
                    OutputSamples[Frame * 2] = Int16PCM[Frame];
                    OutputSamples[Frame * 2 + 1] = Int16PCM[Frame];
                } else if (Channels >= 2) {
                    OutputSamples[Frame * 2] = Int16PCM[Frame * Channels];
                    OutputSamples[Frame * 2 + 1] = Int16PCM[Frame * Channels + 1];
                }
            }
        } else {
            std::fill(OutputSamples, OutputSamples + TotalOutSamples, static_cast<int16_t>(0));
        }
    } else {
        float* OutputSamples = reinterpret_cast<float*>(PcmConversionBuffer.data());
        if (InputData == nullptr) {
            std::fill(OutputSamples, OutputSamples + TotalOutSamples, 0.0f);
        } else if (WVFormatFloat && BitsPerSample == 32) {
            const float* FloatPCM = reinterpret_cast<const float*>(InputData);
            for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                if (Channels == 1) {
                    OutputSamples[Frame * 2] = FloatPCM[Frame];
                    OutputSamples[Frame * 2 + 1] = FloatPCM[Frame];
                } else if (Channels >= 2) {
                    OutputSamples[Frame * 2] = FloatPCM[Frame * Channels];
                    OutputSamples[Frame * 2 + 1] = FloatPCM[Frame * Channels + 1];
                }
            }
        }
    }

    AudioFrameHeader Header;
    Header.Liss = 0x4F4D4E49;
    Header.SampleRate = SampleRate;
    Header.Channels = static_cast<uint16_t>(OutputChannels);
    Header.BitsPerSample = S16NormalizerState ? 16 : 32;
    Header.AudioCodecType = S16NormalizerState ? CodecType::PCM_S16LE : CodecType::PCM_FLOAT32;
    Header.PayloadSize = static_cast<uint32_t>(PcmPayloadSize);

    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    Header.TimestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    Header.FrameIndex = FrameIndex++;

    size_t totalPacketSize = sizeof(AudioFrameHeader) + PcmPayloadSize;
    if (PacketBuffer.size() < totalPacketSize) {
        PacketBuffer.resize(totalPacketSize);
    }

    std::memcpy(PacketBuffer.data(), &Header, sizeof(AudioFrameHeader));
    std::memcpy(
        PacketBuffer.data() + sizeof(AudioFrameHeader), PcmConversionBuffer.data(), PcmPayloadSize
    );

    if (Callback) {
        Callback(PacketBuffer.data(), totalPacketSize, Header);
    }
}
