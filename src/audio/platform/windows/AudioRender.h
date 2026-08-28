#ifndef AUDIORENDER_H
#define AUDIORENDER_H

#pragma once

#include "AudioCap.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;

enum class AudioRenderState : uint8_t { Inactive = 0, Active = 1, Paused = 2 };

class AudioRender
{
  public:
    AudioRender();
    ~AudioRender();

    AudioRender(const AudioRender&) = delete;
    AudioRender& operator=(const AudioRender&) = delete;

    bool Init(uint32_t InSampleRate = 48000, uint16_t InChannels = 2);

    bool Start();
    void Stop();
    void Pause();
    void Resume();

    void WritePacket(const uint8_t* PacketData, size_t PacketSize);

    void GetBufferPool(
        char*&    DataPtr,
        uint32_t& DataSize,
        uint32_t& SlotCount,
        void (**OnSlotComplete)(void*, uint32_t, uint32_t),
        void*& Ctx
    );

    AudioRenderState GetState() const { return RenderState.load(std::memory_order_relaxed); }

    bool Playing() const { return GetState() == AudioRenderState::Active; }
    bool Paused() const { return GetState() == AudioRenderState::Paused; }

    uint32_t GetSampleRate() const { return TargetSampleRate; }
    uint16_t GetChannels() const { return TargetChannels; }

  private:
    void RenderWorkerThread();

    // Pushing samples to the ring buffer in 1 or 2 chunks
    void PushSample(const uint8_t* InData, size_t InByteCount);

    // These all do the same thing just with different sample format combinations
    // 16 is int 16 bit and 32 is 32 bit float, only 32-32 and 16-16 are bit perfect
    // Other 2 go thru conversion before given away to WASAPI
    size_t PopSample32To32(float* OutBuffer, size_t RequestedFrames, uint32_t DeviceChannels);
    size_t PopSample16To32(float* OutBuffer, size_t RequestedFrames, uint32_t DeviceChannels);
    size_t PopSample16To16(int16_t* OutBuffer, size_t RequestedFrames, uint32_t DeviceChannels);
    size_t PopSample32To16(int16_t* OutBuffer, size_t RequestedFrames, uint32_t DeviceChannels);

    uint32_t TargetSampleRate = 48000;
    uint16_t TargetChannels = 2;

    std::atomic<AudioRenderState> RenderState{AudioRenderState::Inactive};
    std::atomic<bool>             StopRequestedState{false};

    // Active incoming audio stream metadata
    std::atomic<CodecType> ActiveCodec{CodecType::PCM_S16LE};
    std::atomic<uint16_t>  ActiveChannels{2};
    std::atomic<uint32_t>  ActiveSampleRate{48000};
    std::atomic<uint8_t>   ActiveBitsPerSample{16};

    std::thread WorkerThread;

    // Lock Free SPSC Raw Byte Ring Buffer , 512 KB tho, power of 2
    static constexpr size_t RingBufferCapacity = 524288;
    static constexpr size_t RingBufferMask = RingBufferCapacity - 1;

    std::vector<uint8_t> SamplesRingBuffer;

    // Aligned onto separate 64-byte L1 cache lines to eliminate False Sharing
    alignas(64) std::atomic<uint64_t> TotalBytesWritten{0};
    alignas(64) std::atomic<uint64_t> TotalBytesRead{0};

    // RecvBuffer Pool
    static constexpr uint32_t RecvSlotCount = 32;
    static constexpr uint32_t RecvSlotSize = 4096;
    std::vector<uint8_t>      RecvPoolBuffer;
};

#endif // AUDIORENDER_H
