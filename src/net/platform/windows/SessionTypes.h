#ifndef SESSIONTYPES_H
#define SESSIONTYPES_H

#pragma once

#include "OmniConfig.h"
#include "OmniTypes.h"

#include <atomic>
#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>

namespace OmniNet {

struct OmniHeader
{
    PacketType PacketType;
    uint8_t Target;
    uint8_t Flags;
};

struct SessionPacketContext
{
    void* UserContext = nullptr;
    uint8_t UniqueKey = 0;
};

// Minimal reinterpret tag cast for packet type identification
struct IOContextTag
{
    OVERLAPPED OVStruct;
    BufferType Type;
};

struct FrameCompletionToken
{
    std::atomic<int> PendingChunks{0};
    void (*OnComplete)(void* Arg1, size_t Arg2) = nullptr;
    void* Arg1 = nullptr;
    size_t Arg2 = 0;
};

struct SEND_BUF
{
    OVERLAPPED OVStruct = {};
    BufferType Type = OP_SEND;
    WSABUF TransmitBuffer[2] = {0};
    FrameCompletionToken* Token = nullptr;
    std::atomic<uint32_t>* InflightCounter = nullptr;
};

struct RECV_BUF
{
    OVERLAPPED OVStruct = {};
    BufferType Type = OP_RECV;
    WSABUF TransmitBuffer = {};
    char* data = nullptr;
    sockaddr_in addr = {0};
    int addr_len = 0;
};

template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize> struct IOContextChunkPool
{
    uint32_t PoolHead = 0;
    ContextType ContextPool[PoolSize];
    char BufferPool[PoolSize * (ChunkSize + 1)] = "";
    uint32_t CurrentChunkUsage = 0;

    uint32_t _mask = PoolSize - 1;

    inline void PushChunk() { PoolHead = (PoolHead + 1) & _mask; }

    inline void PushFinalChunk(uint32_t FinalChunkSize)
    {
        CurrentChunkUsage = PoolHead * ChunkSize;
        CurrentChunkUsage += FinalChunkSize;
    }

    inline bool TryPushFinalChunk(uint32_t FinalChunkSize)
    {
        CurrentChunkUsage = PoolHead * ChunkSize;
        CurrentChunkUsage += FinalChunkSize;
        if (CurrentChunkUsage > PoolSize * ChunkSize) {
            std::cout << "OmniNet Chunk Pool Overflowing : Packet May Be Corrupted !\n";
            PoolHead = 0;
            CurrentChunkUsage = 0;
            return false;
        }
        return true;
    }

    inline void ResetChunk()
    {
        PoolHead = 0;
        CurrentChunkUsage = 0;
    }
};

template <typename ContextType, typename Header, uint32_t PoolSize> struct IOContextTransmitRing
{
    uint32_t PoolHead = 0;
    ContextType ContextPool[PoolSize];
    Header HeaderPool[PoolSize];

    uint32_t _mask = PoolSize - 1;

    inline void PushChunk() { PoolHead = (PoolHead + 1) & _mask; }
};

template <typename ContextType, uint32_t MaxSlots = 8, uint32_t MaxChunks = 2048>
struct IOContextExternalChunkPool
{
    char* Data = nullptr;
    uint32_t SlotSize = 0;
    uint32_t ChunksPerSlot = 0;
    uint32_t SlotCount = 0;

    uint32_t ActiveSlot = 0;
    uint32_t ActiveChunk = 0;
    uint32_t PoolHead = 0;
    void (*OnSlotComplete)(void* ctx, uint32_t slot, uint32_t size) = nullptr;
    void* SlotCompleteCtx = nullptr;

    ContextType ContextPool[MaxSlots * MaxChunks];

    void Init(
        char* DataPtr,
        uint32_t DataSize,
        uint32_t SlotCount,
        void (*onSlotComplete)(void*, uint32_t, uint32_t) = nullptr,
        void* SlotCompletionCtx = nullptr
    )
    {
        Data = DataPtr;
        SlotCount = SlotCount;
        SlotSize = DataSize / SlotCount;
        ChunksPerSlot = SlotSize / OmniMTU;
        ActiveSlot = 0;
        ActiveChunk = 0;
        PoolHead = 0;
        OnSlotComplete = onSlotComplete;
        SlotCompleteCtx = SlotCompletionCtx;

        for (uint32_t slot = 0; slot < SlotCount; ++slot) {
            for (uint32_t chunk = 0; chunk < ChunksPerSlot; ++chunk) {
                const uint32_t idx = slot * ChunksPerSlot + chunk;
                ContextPool[idx].OVStruct = {};
                ContextPool[idx].Type = OP_RECV;
                ContextPool[idx].TransmitBuffer.buf = DataPtr + slot * SlotSize + chunk * OmniMTU;
                ContextPool[idx].TransmitBuffer.len = OmniMTU + OmniHeaderSize;
            }
        }
    }

    inline void PushChunk()
    {
        ++ActiveChunk;
        ++PoolHead;
    }

    inline void PushFinalChunk(uint32_t BufferSize)
    {
        const uint32_t size = ActiveChunk * OmniMTU + BufferSize;
        if (OnSlotComplete) {
            OnSlotComplete(SlotCompleteCtx, ActiveSlot, size);
        }
        ActiveSlot = (ActiveSlot + 1) % SlotCount;
        ActiveChunk = 0;
        PoolHead = ActiveSlot * ChunksPerSlot;
    }

    inline void Reset()
    {
        Data = nullptr;
        SlotSize = 0;
        ChunksPerSlot = 0;
        SlotCount = 0;
        ActiveSlot = 0;
        ActiveChunk = 0;
        PoolHead = 0;
        OnSlotComplete = nullptr;
        SlotCompleteCtx = nullptr;
    }
};

struct SubStreamPoolConfig
{
    char* Data = nullptr;
    uint32_t DataSize = 0;
    uint32_t NumSlots = 0;
    void (*OnSlotComplete)(void* ctx, uint32_t slot, uint32_t size) = nullptr;
    void* Ctx = nullptr;
};

} // namespace OmniNet

#endif // SESSIONTYPES_H
