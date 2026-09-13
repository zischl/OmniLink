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

enum class AudioCaptureMode : uint8_t {
    DesktopOnly    = 0, // System Loopback Output
    MicrophoneOnly = 1, // Microphone Input
    DesktopAndMic  = 2  // Mixed Desktop Audio + Microphone
};

using AudioDeviceType = AudioCaptureMode;

enum class CodecType : uint8_t {
    PCM_S16LE   = 0, // Uncompressed 16-bit signed integer PCM
    PCM_FLOAT32 = 1, // Uncompressed 32-bit float PCM
    LZ4_PCM     = 2, // LZ4 compressed PCM
    OPUS        = 3  // Opus encoded frames
};

#pragma pack(push, 1)
struct AudioFrameHeader
{
    uint32_t  Liss           = 0x4F4D4E49; // Yes.. liss, who ? what ? U don't need to know
    uint32_t  SampleRate     = 48000;
    uint16_t  Channels       = 2;
    uint8_t   BitsPerSample  = 16;
    CodecType AudioCodecType = CodecType::PCM_S16LE;
    uint32_t  PayloadSize    = 0; // Size of payload bytes following header
    uint64_t  TimestampUs    = 0; // Microsecond timestamp
    uint32_t  FrameIndex     = 0; // Incremental frame index
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

    AudioCapture(const AudioCapture&)            = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    bool Init(
        AudioCaptureMode Mode       = AudioCaptureMode::DesktopOnly,
        uint32_t         SampleRate = 48000,
        uint16_t         Channels   = 2
    );

    void SetPacketCallback(AudioPacketCallback InCallback);
    void SetS16NormalizerState(bool State) { S16NormalizerState = State; }

    bool Start();
    void Stop();
    void Pause();
    void Resume();

    // Runtime mode and volume controls
    void             SetCaptureMode(AudioCaptureMode InMode);
    AudioCaptureMode GetCaptureMode() const
    {
        return TargetCaptureMode.load(std::memory_order_relaxed);
    }

    void  SetDesktopVolume(float Volume) { DesktopVolume.store(Volume, std::memory_order_relaxed); }
    float GetDesktopVolume() const { return DesktopVolume.load(std::memory_order_relaxed); }

    void  SetMicVolume(float Volume) { MicVolume.store(Volume, std::memory_order_relaxed); }
    float GetMicVolume() const { return MicVolume.load(std::memory_order_relaxed); }

    void SetMicMuted(bool Muted) { MicMuted.store(Muted, std::memory_order_relaxed); }
    bool GetMicMuteState() const { return MicMuted.load(std::memory_order_relaxed); }

    CaptureState GetState() const { return AudioCapState.load(std::memory_order_relaxed); }
    bool         GetCaptureThreadState() const { return GetState() != CaptureState::Inactive; }
    bool         CapturePaused() const { return GetState() == CaptureState::Paused; }

    uint32_t GetSampleRate() const { return TargetSampleRate; }
    uint16_t GetChannels() const { return TargetChannels; }

  private:
    // Outer state loop to hot swap seamlessly on mode change
    void CaptureWorkerThread();

    // Specialized branchless inner loop capture handlers..
    void RunDesktopCaptureLoop();
    void RunMicCaptureLoop();

    // Desktop + Microphone Mixer Worker
    // Setupp cleanup, Initialize Desktop Loopback and Microphone Capture
    // Drain Microphone into MicRingBuffer and then drain desktop loopback and mix with mic
    void RunDualCaptureLoop();

    // Sample processing routines, this one's for one source mode
    void ProcessSingleSourcePacket(
        const uint8_t*      InputData,
        uint32_t            NumFrames,
        const WAVEFORMATEX* WVFormat,
        float               VolumeScale
    );

    // Sample processing routines, this one's for dual source mode with mixing
    void ProcessDualMixedPacket(
        const float* DAudioSample,
        uint32_t     DAudioFrames,
        uint32_t     DAudioChannels,
        const float* MicFloat,
        uint32_t     MicFrames,
        uint32_t     MicChannels
    );

    std::atomic<AudioCaptureMode> TargetCaptureMode{AudioCaptureMode::DesktopOnly};
    uint32_t                      TargetSampleRate   = 48000;
    uint16_t                      TargetChannels     = 2;
    bool                          S16NormalizerState = true;

    std::atomic<float> DesktopVolume{1.0f};
    std::atomic<float> MicVolume{1.0f};
    std::atomic<bool>  MicMuted{false};

    std::atomic<CaptureState> AudioCapState{CaptureState::Inactive};
    std::atomic<bool>         StopRequestedState{false};

    std::thread         WorkerThread;
    AudioPacketCallback Callback;

    uint32_t             FrameIndex = 0;
    std::vector<uint8_t> PacketBuffer;

    // Ring Buffer for Mic samples during Dual Mode
    // Existing just to close the gap between separate hardware clocks
    static constexpr size_t MicRingBufferCapacity = 65536;
    static constexpr size_t MicRingBufferMask     = MicRingBufferCapacity - 1;
    std::vector<float>      MicRingBuffer;
    alignas(64) std::atomic<uint64_t> MicSamplesWritten{0};
    alignas(64) std::atomic<uint64_t> MicSamplesRead{0};
};

#endif // AUDIOCAP_H
