#ifndef AUDIOCAP_H
#define AUDIOCAP_H

#pragma once

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

enum class AudioDeviceType : uint8_t {
    SystemLoopback = 0, // System Output Audio
    Microphone = 1      // Microphone Input
};

enum class CodecType : uint8_t {
    PCM_S16LE = 0,   // Uncompressed 16-bit signed integer PCM
    PCM_FLOAT32 = 1, // Uncompressed 32-bit float PCM
    LZ4_PCM = 2,     // LZ4 compressed PCM
    OPUS = 3         // Opus encoded frames
};

#pragma pack(push, 1)
struct AudioFrameHeader
{
    uint32_t  Liss = 0x4F4D4E49; // Yes.. liss, who ? what ? U don't need to know
    uint32_t  SampleRate = 48000;
    uint16_t  Channels = 2;
    uint8_t   BitsPerSample = 16;
    CodecType AudioCodecType = CodecType::PCM_S16LE;
    uint32_t  PayloadSize = 0; // Size of payload bytes following header
    uint64_t  TimestampUs = 0; // Microsecond timestamp
    uint32_t  FrameIndex = 0;  // Incremental frame index
};
#pragma pack(pop)

using AudioPacketCallback = std::function<
    void(const uint8_t* PacketData, size_t PacketSize, const AudioFrameHeader& Header)>;

enum class CaptureState : uint8_t { Inactive = 0, Active = 1, Paused = 2 };

class AudioCapture
{
  public:
    AudioCapture();
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool Init(
        AudioDeviceType InDeviceType = AudioDeviceType::SystemLoopback,
        uint32_t        InSampleRate = 48000,
        uint16_t        InChannels = 2
    );
    void SetPacketCallback(AudioPacketCallback InCallback);
    void SetS16NormalizerState(bool State) { S16NormalizerState = State; }

    bool Start();
    void Stop();
    void Pause();
    void Resume();

    CaptureState GetState() const { return AudioCapState.load(std::memory_order_relaxed); }
    bool         GetCaptureThreadState() const { return GetState() != CaptureState::Inactive; }
    bool         CapturePaused() const { return GetState() == CaptureState::Paused; }

    AudioDeviceType GetDeviceType() const { return DeviceType; }
    uint32_t        GetSampleRate() const { return TargetSampleRate; }
    uint16_t        GetChannels() const { return TargetChannels; }

  private:
    void CaptureWorkerThread();
    void ProcessAudioPacket(const uint8_t* InputData, uint32_t NumFrames, const WAVEFORMATEX* Pwfx);

    AudioDeviceType DeviceType = AudioDeviceType::SystemLoopback;
    uint32_t        TargetSampleRate = 48000;
    uint16_t        TargetChannels = 2;
    bool            S16NormalizerState = true;

    std::atomic<CaptureState> AudioCapState{CaptureState::Inactive};
    std::atomic<bool>         StopRequestedState{false};

    std::thread         WorkerThread;
    AudioPacketCallback Callback;

    uint32_t             FrameIndex = 0;
    std::vector<uint8_t> PcmConversionBuffer;
    std::vector<uint8_t> PacketBuffer;
};

#endif // AUDIOCAP_H
