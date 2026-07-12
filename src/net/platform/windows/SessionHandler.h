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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
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

    uint32_t _mask = PoolSize - 1; // Flags

    inline void PushChunk() { PoolHead = (PoolHead + 1) & _mask; }
};

} // namespace OmniNet

class OmniNetContext
{
  private:
    int WSResult;
    int WinsockInit();

  public:
    WSADATA wsaData;

    OmniNetContext();

    static void GetLocals(uint8_t family, std::vector<sockaddr_in>* Buffer);

    static sockaddr_in CreateAddress(PCSTR IP, unsigned short port);

    static SOCKET CreateSocket();

    static bool ConnectSesssion(const sockaddr_in& address, const SOCKET& socketR);

    static HANDLE CreateIOCP(DWORD MaxThreads = 1);

    static bool BindIOCP(HANDLE IOCP, SOCKET Socket, ULONG_PTR CompletionKey);

    template <
        typename ContextType,
        uint32_t PoolSize,
        uint32_t ChunkSize,
        void (*PacketHandlerFn)(char*, uint32_t, uint8_t, void*)>
    static std::thread StartCompletionPortHandlerThread(
        const HANDLE IOCP,
        const SOCKET socket,
        OmniNet::IOContextChunkPool<ContextType, PoolSize, ChunkSize>* Pool,
        void* Ctx
    )
    {

        std::thread StatusQueue([Pool, IOCP, socket, Ctx]() {
            void* Context = Ctx;

            while (true) {
                DWORD BufferSize = 0;
                ULONG_PTR EventKey = 0;
                OVERLAPPED* OVStruct = nullptr;

                bool WSResult =
                    GetQueuedCompletionStatus(IOCP, &BufferSize, &EventKey, &OVStruct, INFINITE);
                if (!WSResult || OVStruct == nullptr) {
                    if (OVStruct == nullptr) {
                        break;
                    }
                    OutputDebugStringA((std::to_string(GetLastError()) + "type \n").c_str());
                    OutputDebugStringA("Completion Status Get False\n");
                }

                // Minimal Packet Type Deduction
                OmniNet::IOContextTag* Tag = reinterpret_cast<OmniNet::IOContextTag*>(OVStruct);

                switch (Tag->Type) {
                case OmniNet::OP_RECV: {
                    OmniNet::RECV_BUF* Buffer = reinterpret_cast<OmniNet::RECV_BUF*>(OVStruct);

                    // header structure's packet type index = 0 and the header size is 3,
                    // going backwards from bytes means minus 2 but since the data stream
                    // is an array it would be minus 3 due to indexing.
                    uint8_t BufferHeader = *(Buffer->TransmitBuffer.buf + BufferSize - 3);

                    switch (BufferHeader) {
                    case OmniNet::PacketType::ChunkStart:
                        Pool->PushChunk();
                        break;
                    case OmniNet::PacketType::ChunkData:
                        Pool->PushChunk();
                        break;
                    case OmniNet::PacketType::ChunkEnd:
                        if (Pool->TryPushFinalChunk(BufferSize)) {
                            PacketHandlerFn(
                                &Pool->BufferPool[0], Pool->CurrentChunkUsage, BufferHeader, Context
                            );
                        }
                        Pool->ResetChunk();
                        break;
                    default:
                        PacketHandlerFn(
                            Buffer->TransmitBuffer.buf, BufferSize, BufferHeader, Context
                        );
                        break;
                    }

                    PostWSARecv(socket, *Pool);
                    break;
                }
                case OmniNet::OP_SEND: {
                    OmniNet::SEND_BUF* SendBuf = reinterpret_cast<OmniNet::SEND_BUF*>(OVStruct);
                    if (SendBuf->Token &&
                        SendBuf->Token->PendingChunks.fetch_sub(1, std::memory_order_acq_rel) ==
                            1) {
                        SendBuf->Token->OnComplete();
                    }
                    break;
                }
                }
            }
        });

        return StatusQueue;
    };

    static bool BindReceiver(PCSTR IP, unsigned int port, SOCKET& socket);

    template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize>
    inline static void PostWSARecv(
        const SOCKET& socket, OmniNet::IOContextChunkPool<ContextType, PoolSize, ChunkSize>& Pool
    )
    {
        DWORD flags = 0;

        int WSResult = WSARecv(
            socket,
            &Pool.ContextPool[Pool.PoolHead].TransmitBuffer,
            1,
            NULL,
            &flags,
            &Pool.ContextPool[Pool.PoolHead].OVStruct,
            NULL
        );

        if (WSAGetLastError() != WSA_IO_PENDING) {
            OutputDebugStringA("Recv Pre Post Failed\n");
        }
    };
};

template <uint32_t MTU = 1450> class OmniNetSession
{
  private:
    int WSResult;
    bool SessionState = false;

    sockaddr_in address;
    SOCKET socketR;
    HANDLE IOCPHandle = NULL;
    std::thread WorkerThread;

    // Usual send queue and header pools
    uint32_t SPoolHead = 0;
    OmniNet::SEND_BUF TransmitPool[256];
    OmniNet::OmniHeader CHeaderPool[256];

    // Tracks how many WSASend OVERLAPPED ops are still owned by the kernel.
    std::atomic<uint32_t> SPoolInflight{0};
    std::atomic<uint32_t> STokenHead{0};
    static constexpr uint32_t STOKEN_POOL_SIZE = 8;
    OmniNet::FrameCompletionToken STokenPool[STOKEN_POOL_SIZE];

    // Lonely Recv pool, don't care about headers, head is built in
    OmniNet::IOContextChunkPool<OmniNet::RECV_BUF, 256, MTU> RecvPool;

    typedef std::chrono::steady_clock Clock;
    typedef std::chrono::time_point<Clock> TimePoint;
    typedef std::chrono::milliseconds Mlliseconds;

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
    OmniNetSession(
        PCSTR Local_IP, PCSTR IP, unsigned short port, void* Context, ULONG_PTR CompletionKey
    )
    {
        PreSetBufferMTU();
        address = OmniNetContext::CreateAddress(IP, port);
        socketR = OmniNetContext::CreateSocket();

        const bool BindState = OmniNetContext::BindReceiver(Local_IP, port, socketR);
        const bool ConnectState = OmniNetContext::ConnectSesssion(address, socketR);

        if (!BindState || !ConnectState) {
            // No IOCP threads for useless sessions :]
            return;
        }

        IOCPHandle = OmniNetContext::CreateIOCP();
        OmniNetContext::BindIOCP(IOCPHandle, socketR, CompletionKey);
        OmniNetContext::PostWSARecv(socketR, RecvPool);
        SessionState = true;
    }

    bool GetSessionState() const { return SessionState; }

    ~OmniNetSession()
    {
        if (IOCPHandle) {
            PostQueuedCompletionStatus(IOCPHandle, 0, 0, nullptr);
        }
        if (WorkerThread.joinable()) {
            WorkerThread.join();
        }
        if (IOCPHandle) {
            CloseHandle(IOCPHandle);
        }
        closesocket(socketR);
    }

    template <void (*PacketHandlerFn)(char*, uint32_t, uint8_t, void*)>
    void SessionStart(void* Context)
    {
        WorkerThread = OmniNetContext::
            StartCompletionPortHandlerThread<OmniNet::RECV_BUF, 256, MTU, PacketHandlerFn>(
                IOCPHandle, socketR, &RecvPool, Context
            );
    }

    // Zero Copy send
    void SessionSend(CHAR* data, int packet_size, const OmniNet::OmniHeader& header)
    {
        while (SPoolInflight.load(std::memory_order_acquire) >= 256) {
            _mm_pause();
        }

        CHeaderPool[SPoolHead].PacketType = header.PacketType;
        CHeaderPool[SPoolHead].Target = header.Target;
        CHeaderPool[SPoolHead].Flags = header.Flags;

        TransmitPool[SPoolHead].TransmitBuffer[1].buf =
            reinterpret_cast<CHAR*>(&CHeaderPool[SPoolHead]);
        TransmitPool[SPoolHead].TransmitBuffer[0].buf = data;
        TransmitPool[SPoolHead].TransmitBuffer[0].len = packet_size;
        TransmitPool[SPoolHead].Token = nullptr;

        SPoolInflight.fetch_add(1, std::memory_order_release);
        WSASend(
            socketR,
            TransmitPool[SPoolHead].TransmitBuffer,
            2,
            NULL,
            0,
            &TransmitPool[SPoolHead].OVStruct,
            NULL
        );

        SPoolHead = (SPoolHead + 1) & 255;
    }

    // Zero-copy chunked send so.. data must remain valid until OnComplete() fires,
    // which happens inside the IOCP thread after the kernel has sent every chunk.
    // OnComplete is optional tho
    void ChunkedSend(CHAR* data, int data_size, std::function<void()> OnComplete)
    {
        const uint32_t TokenIndex =
            STokenHead.fetch_add(1, std::memory_order_relaxed) & (STOKEN_POOL_SIZE - 1);
        OmniNet::FrameCompletionToken& token = STokenPool[TokenIndex];

        while (token.PendingChunks.load(std::memory_order_acquire) != 0) {
            _mm_pause();
        }
        token.OnComplete = std::move(OnComplete);

        const int TotalMTUSlicesSize = data_size - (data_size % MTU);

        const int ChunkCount = (data_size + MTU - 1) / MTU;

        token.PendingChunks.store(ChunkCount, std::memory_order_release);

        auto PostChunk = [&](CHAR* buf, int len, OmniNet::PacketType type) {
            while (SPoolInflight.load(std::memory_order_acquire) >= 256) {
                _mm_pause();
            }
            CHeaderPool[SPoolHead].PacketType = type;
            CHeaderPool[SPoolHead].Target = 0;
            TransmitPool[SPoolHead].TransmitBuffer[0].buf = buf;
            TransmitPool[SPoolHead].TransmitBuffer[0].len = len;
            TransmitPool[SPoolHead].Token = &token;

            SPoolInflight.fetch_add(1, std::memory_order_release);
            WSASend(
                socketR,
                TransmitPool[SPoolHead].TransmitBuffer,
                2,
                NULL,
                0,
                &TransmitPool[SPoolHead].OVStruct,
                NULL
            );

            SPoolHead = (SPoolHead + 1) & 255;
        };

        PostChunk(data, MTU, OmniNet::ChunkStart);

        for (int offset = MTU; offset < TotalMTUSlicesSize - MTU; offset += MTU)
            PostChunk(data + offset, MTU, OmniNet::ChunkData);

        const OmniNet::PacketType penultimateType =
            (TotalMTUSlicesSize != data_size) ? OmniNet::ChunkData : OmniNet::ChunkEnd;
        PostChunk(data + TotalMTUSlicesSize - MTU, MTU, penultimateType);

        if (TotalMTUSlicesSize != data_size)
            PostChunk(data + TotalMTUSlicesSize, data_size % MTU, OmniNet::ChunkEnd);
    }
};

#endif
