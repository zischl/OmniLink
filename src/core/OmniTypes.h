#ifndef OMNITYPES_H
#define OMNITYPES_H

#pragma once

#include <WinSock2.h>
#include <windows.h>
#include <array>
#include <atomic>
#include <functional>
#include <iostream>
#include <optional>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

// Modular sub-headers
#include "OmniEnums.h"
#include "OmniPackets.h"
#include "OmniInstances.h"
#include "BurstQ.h"
#include "AsyncWorker.h"

namespace OmniNet {
enum BufferType : uint8_t { OP_RECV, OP_SEND };

enum PacketType : uint8_t {
  ChunkStart,
  ChunkData,
  ChunkEnd,
  Command,
  ProcMouse,
  ProcKey
};

enum FlagTypes : uint8_t { VoidArg, Argonized };

struct OmniHeader {
  PacketType PacketType;
  uint8_t Target;
  uint8_t Flags;
};

struct SEND_BUF {
  OVERLAPPED OVStruct = {};
  WSABUF TransmitBuffer[2] = {0};
  BufferType Type = OP_SEND;
};

struct RECV_BUF {
  OVERLAPPED OVStruct = {};
  WSABUF TransmitBuffer = {};
  BufferType Type = OP_RECV;
  char *data = nullptr;
  sockaddr_in addr = {0};
  int addr_len = 0;
};

template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize>
struct IOContextChunkPool {
  uint32_t PoolHead = 0;
  ContextType ContextPool[PoolSize];
  char BufferPool[PoolSize * (ChunkSize + 1)] = ""; // extra chunk for safety
  uint32_t CurrentChunkUsage = 0;

  uint32_t _mask = PoolSize - 1; // Flags

  inline void PushChunk() { PoolHead = (PoolHead + 1) & _mask; }

  inline void PushFinalChunk(uint32_t FinalChunkSize) {
    CurrentChunkUsage = PoolHead * ChunkSize;
    CurrentChunkUsage += FinalChunkSize;
  }

  inline bool TryPushFinalChunk(uint32_t FinalChunkSize) {
    CurrentChunkUsage = PoolHead * ChunkSize;
    CurrentChunkUsage += FinalChunkSize;
    if (CurrentChunkUsage > PoolSize * ChunkSize) {
      std::cout
          << "OmniNet Chunk Pool Overflowing : Packet May Be Corrupted !\n";
      PoolHead = 0;
      CurrentChunkUsage = 0;
      return false;
    }
    return true;
  }

  inline void ResetChunk() {
    PoolHead = 0;
    CurrentChunkUsage = 0;
  }
};

template <typename ContextType, typename Header, uint32_t PoolSize>
struct IOContextTransmitRing {
  uint32_t PoolHead = 0;
  ContextType ContextPool[PoolSize];
  Header HeaderPool[PoolSize];

  uint32_t _mask = PoolSize - 1; // Flags

  inline void PushChunk() { PoolHead = (PoolHead + 1) & _mask; }
};
} // namespace OmniNet

#endif // OMNITYPES_H
