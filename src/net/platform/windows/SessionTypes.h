#ifndef SESSIONTYPES_H
#define SESSIONTYPES_H

#pragma once

#include "OmniTypes.h"

#include <atomic>
#include <functional>
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

// Minimal reinterpret tag cast for packet type identification
struct IOContextTag
{
    OVERLAPPED OVStruct;
    BufferType Type;
};

// Still experimental, will update later, runs OnComplete on chunk send completion
struct FrameCompletionToken
{
    std::atomic<int> PendingChunks{0};
    std::function<void()> OnComplete;
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

} // namespace OmniNet

#endif // SESSIONTYPES_H
