#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H

#pragma once
#include "IOLink.h"
#include "OmniConfig.h"
#include "OmniTypes.h"

#include <winsock2.h>

#include <iphlpapi.h>
#include <stdio.h>
#include <ws2tcpip.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#define MEMALLOC(size) HeapAlloc(GetProcessHeap(), 0, size)
#define FREE(size) HeapFree(GetProcessHeap(), 0, size)

namespace OmniNet {

struct OmniHeader
{
    PacketType PacketType;
    uint8_t Target;
    uint8_t Flags;
};

struct SEND_BUF
{
    OVERLAPPED OVStruct = {};
    WSABUF TransmitBuffer[2] = {0};
    BufferType Type = OP_SEND;
};

struct RECV_BUF
{
    OVERLAPPED OVStruct = {};
    WSABUF TransmitBuffer = {};
    BufferType Type = OP_RECV;
    char* data = nullptr;
    sockaddr_in addr = {0};
    int addr_len = 0;
};

template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize> struct IOContextChunkPool
{
    uint32_t PoolHead = 0;
    ContextType ContextPool[PoolSize];
    char BufferPool[PoolSize * (ChunkSize + 1)] = ""; // extra chunk for safety
    uint32_t CurrentChunkUsage = 0;

    uint32_t _mask = PoolSize - 1; // Flags

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

    uint32_t _mask = PoolSize - 1; // Flags

    inline void PushChunk() { PoolHead = (PoolHead + 1) & _mask; }
};

} // namespace OmniNet

class sessions
{
  private:
    int WSResult;

    int WinsockInit();

  public:
    WSADATA wsaData;
    HANDLE IOCP = NULL;

    sessions();

    static void GetLocals(uint8_t family, std::vector<sockaddr_in>* Buffer);

    static sockaddr_in CreateAddress(PCSTR IP, unsigned short port);

    static SOCKET CreateSocket();

    static void ConnectSesssion(const sockaddr_in& address, const SOCKET& socketR);

    static void RegIOCP(HANDLE& IOCP, SOCKET& socket, const ULONG_PTR CompletionKey = 0);

    template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize, typename PacketHandler>
    static void StartCompletionPortHandlerThread(
        const HANDLE& IOCP,
        const SOCKET& socket,
        OmniNet::IOContextChunkPool<ContextType, PoolSize, ChunkSize>& Pool,
        PacketHandler&& PacketHandlerFn,
        void* Ctx)
    {

        std::thread StatusQueue([&, Ctx]() {
            void* Context = Ctx;

            while (true) {
                DWORD BufferSize = 0;
                ULONG_PTR EventKey = 0;
                OVERLAPPED* OVStruct = nullptr;

                bool WSResult =
                    GetQueuedCompletionStatus(IOCP, &BufferSize, &EventKey, &OVStruct, INFINITE);
                if (!WSResult) {
                    OutputDebugStringA((std::to_string(GetLastError()) + "type \n").c_str());
                    OutputDebugStringA("Completion Status Get False\n");
                    if (OVStruct != nullptr) {
                        OutputDebugStringA("Completion Status Get OVStruct Failed\n");
                    } else {
                        OutputDebugStringA("IOCP Thread Going Down...\n");
                    }
                }

                OmniNet::RECV_BUF* Buffer = reinterpret_cast<OmniNet::RECV_BUF*>(OVStruct);

                switch (Buffer->Type) {
                case OmniNet::OP_RECV: {
                    // header structure's packet type index = 0 and the header
                    // size is 3,
                    // going backwards from bytes means minus 2 but since the data stream
                    // is an array it would be minus 3 due to indexing.
                    uint8_t BufferHeader = *(Buffer->TransmitBuffer.buf + BufferSize - 3);

                    switch (BufferHeader) {
                    case OmniNet::PacketType::ChunkStart:
                        Pool.PushChunk();
                        break;
                    case OmniNet::PacketType::ChunkData:
                        Pool.PushChunk();
                        break;
                    case OmniNet::PacketType::ChunkEnd:
                        if (Pool.TryPushFinalChunk(BufferSize)) {
                            PacketHandlerFn(
                                &Pool.BufferPool[0], Pool.CurrentChunkUsage, BufferHeader, Context);
                        }
                        Pool.ResetChunk();
                        break;
                    default:
                        PacketHandlerFn(
                            Buffer->TransmitBuffer.buf, BufferSize, BufferHeader, Context);
                        // Pool.ResetChunk();
                        break;
                    }

                    PostWSARecv(socket, Pool);
                    break;
                }
                case OmniNet::OP_SEND:
                    break;
                }
            }
        });

        StatusQueue.detach();
    };

    static void BindReceiver(PCSTR IP, unsigned int port, SOCKET& socket);

    template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize>
    inline static void
    PostWSARecv(const SOCKET& socket,
                OmniNet::IOContextChunkPool<ContextType, PoolSize, ChunkSize>& Pool)
    {
        DWORD flags = 0;

        int WSResult = WSARecv(socket,
                               &Pool.ContextPool[Pool.PoolHead].TransmitBuffer,
                               1,
                               NULL,
                               &flags,
                               &Pool.ContextPool[Pool.PoolHead].OVStruct,
                               NULL);

        if (WSAGetLastError() != WSA_IO_PENDING) {
            OutputDebugStringA("Recv Pre Post Failed\n");
        }
    };
};

class session
{
  private:
    int WSResult;

    sockaddr_in address;
    SOCKET socketR;

    const int MTU;

    uint32_t SPoolHead = 0;
    OmniNet::SEND_BUF TransmitPool[256];
    OmniNet::OmniHeader CHeaderPool[256];

    OmniNet::IOContextChunkPool<OmniNet::RECV_BUF, 256, 1450> RecvPool;

    typedef std::chrono::steady_clock Clock;
    typedef std::chrono::time_point<Clock> TimePoint;
    typedef std::chrono::milliseconds Mlliseconds;
    TimePoint start;

    inline void PreSetBufferMTU()
    {
        int addr_size = sizeof(sockaddr_in);

        for (int BufferCount = 0; BufferCount < 256; BufferCount++) {
            TransmitPool[BufferCount].OVStruct = {};
            TransmitPool[BufferCount].Type = OmniNet::OP_SEND;
            TransmitPool[BufferCount].TransmitBuffer[0].len = MTU;
            TransmitPool[BufferCount].TransmitBuffer[1].len = OmniHeaderSize;
            TransmitPool[BufferCount].TransmitBuffer[1].buf =
                reinterpret_cast<CHAR*>(&CHeaderPool[BufferCount]);

            RecvPool.ContextPool[BufferCount].OVStruct = {};
            RecvPool.ContextPool[BufferCount].data = &RecvPool.BufferPool[BufferCount * (MTU)];
            RecvPool.ContextPool[BufferCount].TransmitBuffer.buf =
                RecvPool.ContextPool[BufferCount].data;
            RecvPool.ContextPool[BufferCount].TransmitBuffer.len = MTU + OmniHeaderSize;
            RecvPool.ContextPool[BufferCount].Type = OmniNet::OP_RECV;
            RecvPool.ContextPool[BufferCount].addr_len = addr_size;
        }
    }

  public:
    session(
        HANDLE& IOCP, PCSTR Local_IP, PCSTR IP, unsigned short port, int MTU_Size, void* Context);

    void (*OnIOCompletion)(CHAR* Buffer,
                           DWORD BufferSize,
                           uint8_t BufferHeader,
                           void* Context) = nullptr;

    void SessionSend(CHAR* data, int packet_size, const OmniNet::OmniHeader& header);

    void ChunkedSend(CHAR* data, int data_size);
};

#endif
