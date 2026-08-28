#include "AudioCap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

AudioCapture::AudioCapture()
{
    MicRingBuffer.resize(MicRingBufferCapacity, 0.0f);
}

AudioCapture::~AudioCapture()
{
    Stop();
}

bool AudioCapture::Init(AudioCaptureMode Mode, uint32_t SampleRate, uint16_t Channels)
{
    if (GetState() != CaptureState::Inactive)
        return false;
    TargetCaptureMode.store(Mode, std::memory_order_relaxed);
    TargetSampleRate = SampleRate;
    TargetChannels   = Channels;
    return true;
}

void AudioCapture::SetPacketCallback(AudioPacketCallback APCallback)
{
    Callback = std::move(APCallback);
}

void AudioCapture::SetCaptureMode(AudioCaptureMode Mode)
{
    TargetCaptureMode.store(Mode, std::memory_order_relaxed);
}

bool AudioCapture::Start()
{
    CaptureState Expected = CaptureState::Inactive;
    if (!AudioCapState.compare_exchange_strong(Expected, CaptureState::Active)) {
        return true;
    }
    StopRequestedState = false;
    WorkerThread       = std::thread(&AudioCapture::CaptureWorkerThread, this);
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
    HRESULT HResult       = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    bool    CoInitialized = SUCCEEDED(HResult);

    struct ScopeExit
    {
        std::function<void()> ExitFn;
        ~ScopeExit()
        {
            if (ExitFn)
                ExitFn();
        }
    } CleanupGuard{[&]() {
        if (CoInitialized)
            CoUninitialize();
    }};

    while (!StopRequestedState.load(std::memory_order_relaxed)) {
        AudioCaptureMode CurrentMode = TargetCaptureMode.load(std::memory_order_relaxed);
        switch (CurrentMode) {
        case AudioCaptureMode::DesktopOnly:
            RunDesktopCaptureLoop();
            break;
        case AudioCaptureMode::MicrophoneOnly:
            RunMicCaptureLoop();
            break;
        case AudioCaptureMode::DesktopAndMic:
            RunDualCaptureLoop();
            break;
        }
    }
}

// Desktop Loopback Worker
void AudioCapture::RunDesktopCaptureLoop()
{
    WAVEFORMATEX* WVFormat = nullptr;
    HANDLE        hEvent   = nullptr;

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
    }};

    ComPtr<IMMDeviceEnumerator> Enumerator;
    ComPtr<IMMDevice>           Device;
    ComPtr<IAudioClient>        AudioClient;
    ComPtr<IAudioClient3>       AudioClient3;
    ComPtr<IAudioCaptureClient> CaptureClient;

    if (FAILED(CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            NULL,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&Enumerator
        )))
        return;

    if (FAILED(Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &Device)))
        return;

    if (SUCCEEDED(
            Device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)&AudioClient3)
        )) {
        AudioClient = AudioClient3;
    } else {
        if (FAILED(
                Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&AudioClient)
            ))
            return;
    }

    if (FAILED(AudioClient->GetMixFormat(&WVFormat)))
        return;

    DWORD StreamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK;

    bool InitializedWithClient3 = false;

    if (AudioClient3) {
        UINT32  defaultPeriod = 0, fundamentalPeriod = 0, minPeriod = 0, maxPeriod = 0;
        HRESULT hrPeriod = AudioClient3->GetSharedModeEnginePeriod(
            WVFormat, &defaultPeriod, &fundamentalPeriod, &minPeriod, &maxPeriod
        );
        if (SUCCEEDED(hrPeriod) && minPeriod > 0) {
            if (SUCCEEDED(AudioClient3->InitializeSharedAudioStream(
                    StreamFlags, minPeriod, WVFormat, NULL
                ))) {
                InitializedWithClient3 = true;
            }
        }
    }

    if (!InitializedWithClient3) {
        REFERENCE_TIME RequestedDuration = REFTIMES_PER_MILLISEC * 20;
        if (FAILED(AudioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED, StreamFlags, RequestedDuration, 0, WVFormat, NULL
            )))
            return;
    }

    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hEvent)
        return;

    if (FAILED(AudioClient->SetEventHandle(hEvent)))
        return;
    if (FAILED(AudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&CaptureClient)))
        return;
    if (FAILED(AudioClient->Start()))
        return;

    while (!StopRequestedState.load(std::memory_order_relaxed) &&
           TargetCaptureMode.load(std::memory_order_relaxed) == AudioCaptureMode::DesktopOnly) {
        DWORD WaitResult = WaitForSingleObject(hEvent, 50);
        if (StopRequestedState.load(std::memory_order_relaxed))
            break;

        if (WaitResult == WAIT_OBJECT_0) {
            UINT32  PacketLength = 0;
            HRESULT HResult      = CaptureClient->GetNextPacketSize(&PacketLength);
            while (SUCCEEDED(HResult) && PacketLength > 0 &&
                   !StopRequestedState.load(std::memory_order_relaxed)) {
                BYTE*  Data               = nullptr;
                UINT32 NumFramesAvailable = 0;
                DWORD  Flags              = 0;

                HResult = CaptureClient->GetBuffer(&Data, &NumFramesAvailable, &Flags, NULL, NULL);
                if (FAILED(HResult))
                    break;

                if (GetState() != CaptureState::Paused) {
                    float Vol = DesktopVolume.load(std::memory_order_relaxed);
                    if (Flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        ProcessSingleSourcePacket(nullptr, NumFramesAvailable, WVFormat, Vol);
                    } else {
                        ProcessSingleSourcePacket(Data, NumFramesAvailable, WVFormat, Vol);
                    }
                }

                CaptureClient->ReleaseBuffer(NumFramesAvailable);
                HResult = CaptureClient->GetNextPacketSize(&PacketLength);
            }
        }
    }

    AudioClient->Stop();
}

// Microphone Worker
void AudioCapture::RunMicCaptureLoop()
{
    WAVEFORMATEX* WVFormat = nullptr;
    HANDLE        hEvent   = nullptr;

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
    }};

    ComPtr<IMMDeviceEnumerator> Enumerator;
    ComPtr<IMMDevice>           Device;
    ComPtr<IAudioClient>        AudioClient;
    ComPtr<IAudioClient3>       AudioClient3;
    ComPtr<IAudioCaptureClient> CaptureClient;

    if (FAILED(CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            NULL,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&Enumerator
        )))
        return;

    if (FAILED(Enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &Device)))
        return;

    if (SUCCEEDED(
            Device->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, NULL, (void**)&AudioClient3)
        )) {
        AudioClient = AudioClient3;
    } else {
        if (FAILED(
                Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&AudioClient)
            ))
            return;
    }

    if (FAILED(AudioClient->GetMixFormat(&WVFormat)))
        return;

    DWORD StreamFlags            = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    bool  InitializedWithClient3 = false;

    if (AudioClient3) {
        UINT32  defaultPeriod = 0, fundamentalPeriod = 0, minPeriod = 0, maxPeriod = 0;
        HRESULT hrPeriod = AudioClient3->GetSharedModeEnginePeriod(
            WVFormat, &defaultPeriod, &fundamentalPeriod, &minPeriod, &maxPeriod
        );
        if (SUCCEEDED(hrPeriod) && minPeriod > 0) {
            if (SUCCEEDED(AudioClient3->InitializeSharedAudioStream(
                    StreamFlags, minPeriod, WVFormat, NULL
                ))) {
                InitializedWithClient3 = true;
            }
        }
    }

    if (!InitializedWithClient3) {
        REFERENCE_TIME RequestedDuration = REFTIMES_PER_MILLISEC * 20;
        if (FAILED(AudioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED, StreamFlags, RequestedDuration, 0, WVFormat, NULL
            )))
            return;
    }

    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hEvent)
        return;

    if (FAILED(AudioClient->SetEventHandle(hEvent)))
        return;
    if (FAILED(AudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&CaptureClient)))
        return;
    if (FAILED(AudioClient->Start()))
        return;

    while (!StopRequestedState.load(std::memory_order_relaxed) &&
           TargetCaptureMode.load(std::memory_order_relaxed) == AudioCaptureMode::MicrophoneOnly) {
        DWORD WaitResult = WaitForSingleObject(hEvent, 50);
        if (StopRequestedState.load(std::memory_order_relaxed))
            break;

        if (WaitResult == WAIT_OBJECT_0) {
            UINT32  PacketLength = 0;
            HRESULT HResult      = CaptureClient->GetNextPacketSize(&PacketLength);
            while (SUCCEEDED(HResult) && PacketLength > 0 &&
                   !StopRequestedState.load(std::memory_order_relaxed)) {
                BYTE*  Data               = nullptr;
                UINT32 NumFramesAvailable = 0;
                DWORD  Flags              = 0;

                HResult = CaptureClient->GetBuffer(&Data, &NumFramesAvailable, &Flags, NULL, NULL);
                if (FAILED(HResult))
                    break;

                if (GetState() != CaptureState::Paused) {
                    float MicVol        = MicVolume.load(std::memory_order_relaxed);
                    float MicMuteFactor = MicMuted.load(std::memory_order_relaxed) ? 0.0f : 1.0f;
                    float EffectiveVol  = MicVol * MicMuteFactor;

                    if (Flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        ProcessSingleSourcePacket(
                            nullptr, NumFramesAvailable, WVFormat, EffectiveVol
                        );
                    } else {
                        ProcessSingleSourcePacket(Data, NumFramesAvailable, WVFormat, EffectiveVol);
                    }
                }

                CaptureClient->ReleaseBuffer(NumFramesAvailable);
                HResult = CaptureClient->GetNextPacketSize(&PacketLength);
            }
        }
    }

    AudioClient->Stop();
}

void AudioCapture::RunDualCaptureLoop()
{
    WAVEFORMATEX* DeskWVFormat = nullptr;
    WAVEFORMATEX* MicWVFormat  = nullptr;
    HANDLE        hDeskEvent   = nullptr;
    HANDLE        hMicEvent    = nullptr;

    struct ScopeExit
    {
        std::function<void()> ExitFn;
        ~ScopeExit()
        {
            if (ExitFn)
                ExitFn();
        }
    } CleanupGuard{[&]() {
        if (DeskWVFormat)
            CoTaskMemFree(DeskWVFormat);
        if (MicWVFormat)
            CoTaskMemFree(MicWVFormat);
        if (hDeskEvent)
            CloseHandle(hDeskEvent);
        if (hMicEvent)
            CloseHandle(hMicEvent);
    }};

    ComPtr<IMMDeviceEnumerator> Enumerator;
    ComPtr<IMMDevice>           DesktopAudioDevice;
    ComPtr<IMMDevice>           MicDevice;
    ComPtr<IAudioClient>        DesktopAudioClient;
    ComPtr<IAudioClient>        MicAudioClient;
    ComPtr<IAudioCaptureClient> DeskCaptureClient;
    ComPtr<IAudioCaptureClient> MicCaptureClient;

    if (FAILED(CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            NULL,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            (void**)&Enumerator
        )))
        return;

    if (FAILED(Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &DesktopAudioDevice)))
        return;
    if (FAILED(DesktopAudioDevice->Activate(
            __uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&DesktopAudioClient
        )))
        return;
    if (FAILED(DesktopAudioClient->GetMixFormat(&DeskWVFormat)))
        return;

    REFERENCE_TIME RequestedDuration = REFTIMES_PER_MILLISEC * 20;
    if (FAILED(DesktopAudioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_LOOPBACK,
            RequestedDuration,
            0,
            DeskWVFormat,
            NULL
        )))
        return;

    hDeskEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!hDeskEvent)
        return;
    if (FAILED(DesktopAudioClient->SetEventHandle(hDeskEvent)))
        return;
    if (FAILED(DesktopAudioClient->GetService(
            __uuidof(IAudioCaptureClient), (void**)&DeskCaptureClient
        )))
        return;

    bool DeviceMicState =
        SUCCEEDED(Enumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &MicDevice)) &&
        SUCCEEDED(
            MicDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&MicAudioClient)
        ) &&
        SUCCEEDED(MicAudioClient->GetMixFormat(&MicWVFormat));

    if (DeviceMicState) {
        if (SUCCEEDED(MicAudioClient->Initialize(
                AUDCLNT_SHAREMODE_SHARED,
                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                RequestedDuration,
                0,
                MicWVFormat,
                NULL
            ))) {
            hMicEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (hMicEvent && SUCCEEDED(MicAudioClient->SetEventHandle(hMicEvent)) &&
                SUCCEEDED(MicAudioClient->GetService(
                    __uuidof(IAudioCaptureClient), (void**)&MicCaptureClient
                ))) {
                MicAudioClient->Start();
            } else {
                DeviceMicState = false;
            }
        } else {
            DeviceMicState = false;
        }
    }

    if (FAILED(DesktopAudioClient->Start())) {
        if (DeviceMicState)
            MicAudioClient->Stop();
        return;
    }

    HANDLE WaitHandles[2] = {hDeskEvent, hMicEvent ? hMicEvent : hDeskEvent};
    DWORD  HandleCount    = DeviceMicState ? 2 : 1;

    std::vector<float> MicPopBuffer;

    while (!StopRequestedState.load(std::memory_order_relaxed) &&
           TargetCaptureMode.load(std::memory_order_relaxed) == AudioCaptureMode::DesktopAndMic) {
        DWORD WaitResult = WaitForMultipleObjects(HandleCount, WaitHandles, FALSE, 50);
        if (StopRequestedState.load(std::memory_order_relaxed))
            break;

        if (DeviceMicState) {
            UINT32  MicPacketLength = 0;
            HRESULT hrMic           = MicCaptureClient->GetNextPacketSize(&MicPacketLength);
            while (SUCCEEDED(hrMic) && MicPacketLength > 0) {
                BYTE*  MicData   = nullptr;
                UINT32 MicFrames = 0;
                DWORD  MicFlags  = 0;

                if (FAILED(
                        MicCaptureClient->GetBuffer(&MicData, &MicFrames, &MicFlags, NULL, NULL)
                    ))
                    break;

                if (MicFrames > 0) {
                    uint64_t Written  = MicSamplesWritten.load(std::memory_order_relaxed);
                    size_t   WriteIdx = static_cast<size_t>(Written & MicRingBufferMask);

                    if (MicFlags & AUDCLNT_BUFFERFLAGS_SILENT || MicData == nullptr) {
                        for (uint32_t Frame = 0; Frame < MicFrames; ++Frame) {
                            MicRingBuffer[(WriteIdx + Frame * 2) & MicRingBufferMask]     = 0.0f;
                            MicRingBuffer[(WriteIdx + Frame * 2 + 1) & MicRingBufferMask] = 0.0f;
                        }
                    } else if (MicWVFormat->wBitsPerSample == 32) {
                        const float* MicDataFloatSample = reinterpret_cast<const float*>(MicData);
                        if (MicWVFormat->nChannels == 1) {
                            for (uint32_t Frame = 0; Frame < MicFrames; ++Frame) {
                                float MicSampleFrame = MicDataFloatSample[Frame];
                                MicRingBuffer[(WriteIdx + Frame * 2) & MicRingBufferMask] =
                                    MicSampleFrame;
                                MicRingBuffer[(WriteIdx + Frame * 2 + 1) & MicRingBufferMask] =
                                    MicSampleFrame;
                            }
                        } else {
                            for (uint32_t f = 0; f < MicFrames; ++f) {
                                MicRingBuffer[(WriteIdx + f * 2) & MicRingBufferMask] =
                                    MicDataFloatSample[f * MicWVFormat->nChannels];
                                MicRingBuffer[(WriteIdx + f * 2 + 1) & MicRingBufferMask] =
                                    MicDataFloatSample[f * MicWVFormat->nChannels + 1];
                            }
                        }
                    } else if (MicWVFormat->wBitsPerSample == 16) {
                        const int16_t* MicDataIntSample = reinterpret_cast<const int16_t*>(MicData);
                        constexpr float Scale           = 1.0f / 32768.0f;
                        if (MicWVFormat->nChannels == 1) {
                            for (uint32_t Frame = 0; Frame < MicFrames; ++Frame) {
                                float MicSampleFrame =
                                    static_cast<float>(MicDataIntSample[Frame]) * Scale;
                                MicRingBuffer[(WriteIdx + Frame * 2) & MicRingBufferMask] =
                                    MicSampleFrame;
                                MicRingBuffer[(WriteIdx + Frame * 2 + 1) & MicRingBufferMask] =
                                    MicSampleFrame;
                            }
                        } else {
                            for (uint32_t f = 0; f < MicFrames; ++f) {
                                MicRingBuffer[(WriteIdx + f * 2) & MicRingBufferMask] =
                                    static_cast<float>(
                                        MicDataIntSample[f * MicWVFormat->nChannels]
                                    ) *
                                    Scale;
                                MicRingBuffer[(WriteIdx + f * 2 + 1) & MicRingBufferMask] =
                                    static_cast<float>(
                                        MicDataIntSample[f * MicWVFormat->nChannels + 1]
                                    ) *
                                    Scale;
                            }
                        }
                    }
                    MicSamplesWritten.store(Written + MicFrames * 2, std::memory_order_release);
                }

                MicCaptureClient->ReleaseBuffer(MicFrames);
                hrMic = MicCaptureClient->GetNextPacketSize(&MicPacketLength);
            }
        }

        UINT32  DAudioPacketLength = 0;
        HRESULT HRDesktopAudio     = DeskCaptureClient->GetNextPacketSize(&DAudioPacketLength);
        while (SUCCEEDED(HRDesktopAudio) && DAudioPacketLength > 0 &&
               !StopRequestedState.load(std::memory_order_relaxed)) {
            BYTE*  DAudioData   = nullptr;
            UINT32 DAudioFrames = 0;
            DWORD  DAudioFlags  = 0;

            if (FAILED(DeskCaptureClient->GetBuffer(
                    &DAudioData, &DAudioFrames, &DAudioFlags, NULL, NULL
                )))
                break;

            if (GetState() != CaptureState::Paused && DAudioFrames > 0) {
                size_t RequiredMicFloats = DAudioFrames * 2;
                if (MicPopBuffer.size() < RequiredMicFloats) {
                    MicPopBuffer.resize(RequiredMicFloats, 0.0f);
                }

                uint64_t MicWritten = MicSamplesWritten.load(std::memory_order_acquire);
                uint64_t MicRead    = MicSamplesRead.load(std::memory_order_relaxed);
                uint64_t Available  = (MicWritten >= MicRead) ? (MicWritten - MicRead) : 0;
                size_t   PopCount   = (std::min)(RequiredMicFloats, static_cast<size_t>(Available));

                if (PopCount > 0) {
                    size_t ReadIdx = static_cast<size_t>(MicRead & MicRingBufferMask);
                    for (size_t i = 0; i < PopCount; ++i) {
                        MicPopBuffer[i] = MicRingBuffer[(ReadIdx + i) & MicRingBufferMask];
                    }
                    MicSamplesRead.store(MicRead + PopCount, std::memory_order_release);
                }
                if (PopCount < RequiredMicFloats) {
                    std::fill(
                        MicPopBuffer.data() + PopCount,
                        MicPopBuffer.data() + RequiredMicFloats,
                        0.0f
                    );
                }

                const float* DesktopAudioSample =
                    (DAudioFlags & AUDCLNT_BUFFERFLAGS_SILENT || DAudioData == nullptr)
                        ? nullptr
                        : reinterpret_cast<const float*>(DAudioData);

                ProcessDualMixedPacket(
                    DesktopAudioSample,
                    DAudioFrames,
                    DeskWVFormat->nChannels,
                    MicPopBuffer.data(),
                    DAudioFrames,
                    2
                );
            }

            DeskCaptureClient->ReleaseBuffer(DAudioFrames);
            HRDesktopAudio = DeskCaptureClient->GetNextPacketSize(&DAudioPacketLength);
        }
    }

    DesktopAudioClient->Stop();
    if (DeviceMicState) {
        MicAudioClient->Stop();
    }
}

// Single Source Packet Processor
void AudioCapture::ProcessSingleSourcePacket(
    const uint8_t* InputData, uint32_t NumFrames, const WAVEFORMATEX* WVFormat, float VolumeScale
)
{
    if (NumFrames == 0 || !WVFormat)
        return;

    WORD  Channels      = WVFormat->nChannels;
    WORD  BitsPerSample = WVFormat->wBitsPerSample;
    DWORD SampleRate    = WVFormat->nSamplesPerSec;

    bool WVFormatFloat = (WVFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                         (WVFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                          reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(WVFormat)->SubFormat ==
                              KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);

    uint32_t OutputChannels  = TargetChannels;
    size_t   TotalOutSamples = static_cast<size_t>(NumFrames) * OutputChannels;
    size_t   PcmPayloadSize  = S16NormalizerState ? (TotalOutSamples * sizeof(int16_t))
                                                  : (TotalOutSamples * sizeof(float));

    size_t TotalPacketSize = sizeof(AudioFrameHeader) + PcmPayloadSize;
    if (PacketBuffer.size() < TotalPacketSize) {
        PacketBuffer.resize(TotalPacketSize);
    }

    uint8_t* PayloadPtr = PacketBuffer.data() + sizeof(AudioFrameHeader);

    if (S16NormalizerState) {
        int16_t* OutputSamples = reinterpret_cast<int16_t*>(PayloadPtr);

        if (InputData == nullptr) {
            std::fill(OutputSamples, OutputSamples + TotalOutSamples, static_cast<int16_t>(0));
        } else if (WVFormatFloat && BitsPerSample == 32) {
            const float* FloatPCM = reinterpret_cast<const float*>(InputData);
            // May the god of hardware acceleration bless this block
            if (Channels == 2) {
                for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                    float Left =
                        (std::max)(-1.0f, (std::min)(1.0f, FloatPCM[Frame * 2] * VolumeScale));
                    float Right =
                        (std::max)(-1.0f, (std::min)(1.0f, FloatPCM[Frame * 2 + 1] * VolumeScale));
                    OutputSamples[Frame * 2]     = static_cast<int16_t>(Left * 32767.0f);
                    OutputSamples[Frame * 2 + 1] = static_cast<int16_t>(Right * 32767.0f);
                }
            } else if (Channels == 1) {
                for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                    float Mono = (std::max)(-1.0f, (std::min)(1.0f, FloatPCM[Frame] * VolumeScale));
                    int16_t Val                  = static_cast<int16_t>(Mono * 32767.0f);
                    OutputSamples[Frame * 2]     = Val;
                    OutputSamples[Frame * 2 + 1] = Val;
                }
            } else {
                for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                    float Left =
                        (std::max)(-1.0f,
                                   (std::min)(1.0f, FloatPCM[Frame * Channels] * VolumeScale));
                    float Right =
                        (std::max)(-1.0f,
                                   (std::min)(1.0f, FloatPCM[Frame * Channels + 1] * VolumeScale));
                    OutputSamples[Frame * 2]     = static_cast<int16_t>(Left * 32767.0f);
                    OutputSamples[Frame * 2 + 1] = static_cast<int16_t>(Right * 32767.0f);
                }
            }
        } else if (!WVFormatFloat && BitsPerSample == 16) {
            const int16_t* Int16PCM = reinterpret_cast<const int16_t*>(InputData);
            if (Channels == 2) {
                if (VolumeScale >= 0.999f) {
                    std::memcpy(OutputSamples, Int16PCM, NumFrames * 2 * sizeof(int16_t));
                } else {
                    for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                        OutputSamples[Frame * 2] =
                            static_cast<int16_t>(Int16PCM[Frame * 2] * VolumeScale);
                        OutputSamples[Frame * 2 + 1] =
                            static_cast<int16_t>(Int16PCM[Frame * 2 + 1] * VolumeScale);
                    }
                }
            } else if (Channels == 1) {
                for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                    int16_t Val              = static_cast<int16_t>(Int16PCM[Frame] * VolumeScale);
                    OutputSamples[Frame * 2] = Val;
                    OutputSamples[Frame * 2 + 1] = Val;
                }
            }
        } else {
            std::fill(OutputSamples, OutputSamples + TotalOutSamples, static_cast<int16_t>(0));
        }
    } else {
        float* OutputSamples = reinterpret_cast<float*>(PayloadPtr);
        if (InputData == nullptr) {
            std::fill(OutputSamples, OutputSamples + TotalOutSamples, 0.0f);
        } else if (WVFormatFloat && BitsPerSample == 32) {
            const float* FloatPCM = reinterpret_cast<const float*>(InputData);
            if (Channels == 2) {
                for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                    OutputSamples[Frame * 2]     = FloatPCM[Frame * 2] * VolumeScale;
                    OutputSamples[Frame * 2 + 1] = FloatPCM[Frame * 2 + 1] * VolumeScale;
                }
            } else if (Channels == 1) {
                for (uint32_t Frame = 0; Frame < NumFrames; ++Frame) {
                    float S                      = FloatPCM[Frame] * VolumeScale;
                    OutputSamples[Frame * 2]     = S;
                    OutputSamples[Frame * 2 + 1] = S;
                }
            }
        }
    }

    AudioFrameHeader Header;
    Header.Liss           = 0x4F4D4E49;
    Header.SampleRate     = SampleRate;
    Header.Channels       = static_cast<uint16_t>(OutputChannels);
    Header.BitsPerSample  = S16NormalizerState ? 16 : 32;
    Header.AudioCodecType = S16NormalizerState ? CodecType::PCM_S16LE : CodecType::PCM_FLOAT32;
    Header.PayloadSize    = static_cast<uint32_t>(PcmPayloadSize);

    auto now           = std::chrono::high_resolution_clock::now().time_since_epoch();
    Header.TimestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    Header.FrameIndex  = FrameIndex++;

    std::memcpy(PacketBuffer.data(), &Header, sizeof(AudioFrameHeader));

    if (Callback) {
        Callback(PacketBuffer.data(), TotalPacketSize, Header);
    }
}

// Dual Source Mixer, yes.. also hopefully hardware vectorized
void AudioCapture::ProcessDualMixedPacket(
    const float* DAudioSample,
    uint32_t     DAudioFrames,
    uint32_t     DAudioChannels,
    const float* MicFloat,
    uint32_t     MicFrames,
    uint32_t     MicChannels
)
{
    if (DAudioFrames == 0)
        return;

    float DAudioVol       = DesktopVolume.load(std::memory_order_relaxed);
    float MicVol          = MicVolume.load(std::memory_order_relaxed);
    float MicMuteFactor   = MicMuted.load(std::memory_order_relaxed) ? 0.0f : 1.0f;
    float EffectiveMicVol = MicVol * MicMuteFactor;

    uint32_t OutputChannels  = TargetChannels;
    size_t   TotalOutSamples = static_cast<size_t>(DAudioFrames) * OutputChannels;
    size_t   PcmPayloadSize  = S16NormalizerState ? (TotalOutSamples * sizeof(int16_t))
                                                  : (TotalOutSamples * sizeof(float));

    size_t TotalPacketSize = sizeof(AudioFrameHeader) + PcmPayloadSize;
    if (PacketBuffer.size() < TotalPacketSize) {
        PacketBuffer.resize(TotalPacketSize);
    }

    uint8_t* PayloadPtr = PacketBuffer.data() + sizeof(AudioFrameHeader);

    if (S16NormalizerState) {
        int16_t* OutputSamples = reinterpret_cast<int16_t*>(PayloadPtr);

        for (uint32_t Frame = 0; Frame < DAudioFrames; ++Frame) {
            float DeskL = (DAudioSample != nullptr) ? DAudioSample[Frame * DAudioChannels] : 0.0f;
            float DeskR = (DAudioSample != nullptr && DAudioChannels >= 2)
                              ? DAudioSample[Frame * DAudioChannels + 1]
                              : DeskL;

            float MicL = (MicFloat != nullptr) ? MicFloat[Frame * MicChannels] : 0.0f;
            float MicR = (MicFloat != nullptr && MicChannels >= 2)
                             ? MicFloat[Frame * MicChannels + 1]
                             : MicL;

            float MixedL =
                (std::max)(-1.0f, (std::min)(1.0f, DeskL * DAudioVol + MicL * EffectiveMicVol));
            float MixedR =
                (std::max)(-1.0f, (std::min)(1.0f, DeskR * DAudioVol + MicR * EffectiveMicVol));

            OutputSamples[Frame * 2]     = static_cast<int16_t>(MixedL * 32767.0f);
            OutputSamples[Frame * 2 + 1] = static_cast<int16_t>(MixedR * 32767.0f);
        }
    } else {
        float* OutputSamples = reinterpret_cast<float*>(PayloadPtr);

        for (uint32_t Frame = 0; Frame < DAudioFrames; ++Frame) {
            float DeskL = (DAudioSample != nullptr) ? DAudioSample[Frame * DAudioChannels] : 0.0f;
            float DeskR = (DAudioSample != nullptr && DAudioChannels >= 2)
                              ? DAudioSample[Frame * DAudioChannels + 1]
                              : DeskL;

            float MicL = (MicFloat != nullptr) ? MicFloat[Frame * MicChannels] : 0.0f;
            float MicR = (MicFloat != nullptr && MicChannels >= 2)
                             ? MicFloat[Frame * MicChannels + 1]
                             : MicL;

            OutputSamples[Frame * 2]     = DeskL * DAudioVol + MicL * EffectiveMicVol;
            OutputSamples[Frame * 2 + 1] = DeskR * DAudioVol + MicR * EffectiveMicVol;
        }
    }

    AudioFrameHeader Header;
    Header.Liss           = 0x4F4D4E49;
    Header.SampleRate     = TargetSampleRate;
    Header.Channels       = static_cast<uint16_t>(OutputChannels);
    Header.BitsPerSample  = S16NormalizerState ? 16 : 32;
    Header.AudioCodecType = S16NormalizerState ? CodecType::PCM_S16LE : CodecType::PCM_FLOAT32;
    Header.PayloadSize    = static_cast<uint32_t>(PcmPayloadSize);

    auto now           = std::chrono::high_resolution_clock::now().time_since_epoch();
    Header.TimestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    Header.FrameIndex  = FrameIndex++;

    std::memcpy(PacketBuffer.data(), &Header, sizeof(AudioFrameHeader));

    if (Callback) {
        Callback(PacketBuffer.data(), TotalPacketSize, Header);
    }
}
