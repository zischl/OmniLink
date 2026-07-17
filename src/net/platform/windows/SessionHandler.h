#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H

#pragma once
#include "OmniConfig.h"
#include "OmniLogger.h"
#include "OmniNetContext.h"
#include "OmniTypes.h"
#include "SessionTypes.h"
// #include "SubStream.h"

#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>

template <uint32_t MTU = OmniMTU> class OmniNetSession
{
  private:
    int WSResult;
    bool SessionState = false;

    char LocalIPString[16] = {};
    sockaddr_in Address;
    SOCKET SocketR;
    HANDLE IOCPHandle = NULL;
    std::thread WorkerThread;

    // Session Identity, will be passed down to given packet handler, later tho.
    uint8_t UniqueKey = 0;

    // Usual send queue and header pools
    static constexpr uint32_t POOL_SIZE = 2048;

    uint32_t SPoolHead = 0;
    OmniNet::SEND_BUF TransmitPool[POOL_SIZE];
    OmniNet::OmniHeader CHeaderPool[POOL_SIZE];

    // Tracks how many WSASend OVERLAPPED ops are still owned by the kernel.
    std::atomic<uint32_t> SPoolInflight{0};
    std::atomic<uint32_t> STokenHead{0};
    static constexpr uint32_t STOKEN_POOL_SIZE = 8;
    OmniNet::FrameCompletionToken STokenPool[STOKEN_POOL_SIZE];

    // Lonely Recv pool, don't care about headers, head is built in
    OmniNet::IOContextChunkPool<OmniNet::RECV_BUF, POOL_SIZE, MTU> RecvPool;

    // UDP Sub Streams, still working on it
    // static constexpr uint32_t MaxSubStreams = 8;
    // OmniNetSubStream SubStreams[MaxSubStreams];

    typedef std::chrono::steady_clock Clock;
    typedef std::chrono::time_point<Clock> TimePoint;
    typedef std::chrono::milliseconds Mlliseconds;

    inline void PreSetBufferMTU()
    {
        int addr_size = sizeof(sockaddr_in);

        for (int BufferCount = 0; BufferCount < (int)POOL_SIZE; BufferCount++) {
            TransmitPool[BufferCount].OVStruct = {};
            TransmitPool[BufferCount].Type = OmniNet::OP_SEND;
            TransmitPool[BufferCount].TransmitBuffer[0].len = MTU;
            TransmitPool[BufferCount].TransmitBuffer[1].len = OmniHeaderSize;
            TransmitPool[BufferCount].TransmitBuffer[1].buf =
                reinterpret_cast<CHAR*>(&CHeaderPool[BufferCount]);
            TransmitPool[BufferCount].InflightCounter = &SPoolInflight;

            RecvPool.ContextPool[BufferCount].OVStruct = {};
            RecvPool.ContextPool[BufferCount].data = &RecvPool.BufferPool[BufferCount * (MTU)];
            RecvPool.ContextPool[BufferCount].TransmitBuffer.buf =
                RecvPool.ContextPool[BufferCount].data;
            RecvPool.ContextPool[BufferCount].TransmitBuffer.len = MTU + OmniHeaderSize;
            RecvPool.ContextPool[BufferCount].Type = OmniNet::OP_RECV;
            RecvPool.ContextPool[BufferCount].addr_len = addr_size;
        }
    }

    inline void PostSessionWSARecv()
    {
        DWORD flags = 0;

        int WSResult = WSARecv(
            SocketR,
            &RecvPool.ContextPool[RecvPool.PoolHead].TransmitBuffer,
            1,
            NULL,
            &flags,
            &RecvPool.ContextPool[RecvPool.PoolHead].OVStruct,
            NULL
        );

        if (WSAGetLastError() != WSA_IO_PENDING) {
            OutputDebugStringA("Recv Pre Post Failed\n");
        }
    }

  public:
    OmniNetSession(PCSTR Local_IP, PCSTR IP, unsigned short port, void* Context, uint8_t SUniqueKey)
    {
        UniqueKey = SUniqueKey;
        strncpy_s(LocalIPString, Local_IP, sizeof(LocalIPString) - 1);

        PreSetBufferMTU();
        Address = OmniNetContext::CreateAddress(IP, port);
        SocketR = OmniNetContext::CreateSocket();

        const bool BindState = OmniNetContext::BindReceiver(Local_IP, port, SocketR);
        const bool ConnectState = OmniNetContext::ConnectSesssion(Address, SocketR);

        if (!BindState || !ConnectState) {
            // No IOCP threads for useless sessions :]
            return;
        }

        IOCPHandle = OmniNetContext::CreateIOCP();
        OmniNetContext::BindIOCP(IOCPHandle, SocketR, 0);
        PostSessionWSARecv();
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
        closesocket(SocketR);
    }

    template <void (*PacketHandlerFn)(char*, uint32_t, uint8_t, void*)>
    void SessionStart(void* Context)
    {
        WorkerThread = std::thread([this, Context]() {
            void* Ctx = Context;

            while (true) {
                DWORD BufferSize = 0;
                ULONG_PTR EventKey = 0;
                OVERLAPPED* OVStruct = nullptr;

                if (!GetQueuedCompletionStatus(
                        IOCPHandle, &BufferSize, &EventKey, &OVStruct, INFINITE
                    )) [[unlikely]] {
                    if (OVStruct == nullptr) {
                        // Logger::log("Completion Status Get False\n", GetLastError());
                        Logger::log("Maybe somthing wrong with the IOCP thread ?");
                        break;
                    }
                }

                if (EventKey != 0) {
                    continue;
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
                        RecvPool.PushChunk();
                        break;
                    case OmniNet::PacketType::ChunkData:
                        RecvPool.PushChunk();
                        break;
                    case OmniNet::PacketType::ChunkEnd:
                        if (RecvPool.TryPushFinalChunk(BufferSize)) {
                            PacketHandlerFn(
                                &RecvPool.BufferPool[0],
                                RecvPool.CurrentChunkUsage,
                                BufferHeader,
                                Ctx
                            );
                        }
                        RecvPool.ResetChunk();
                        break;
                    default:
                        PacketHandlerFn(Buffer->TransmitBuffer.buf, BufferSize, BufferHeader, Ctx);
                        break;
                    }

                    PostSessionWSARecv();
                    break;
                }
                case OmniNet::OP_SEND: {
                    OmniNet::SEND_BUF* SendBuf = reinterpret_cast<OmniNet::SEND_BUF*>(OVStruct);
                    if (SendBuf->Token &&
                        SendBuf->Token->PendingChunks.fetch_sub(1, std::memory_order_acq_rel) ==
                            1) {
                        SendBuf->Token->OnComplete();
                    }
                    if (SendBuf->InflightCounter) {
                        SendBuf->InflightCounter->fetch_sub(1, std::memory_order_release);
                    }
                    break;
                }
                }
            }
        });
    }

    // Zero Copy send
    void SessionSend(CHAR* data, int packet_size, const OmniNet::OmniHeader& header)
    {
        while (SPoolInflight.load(std::memory_order_acquire) >= POOL_SIZE) {
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
            SocketR,
            TransmitPool[SPoolHead].TransmitBuffer,
            2,
            NULL,
            0,
            &TransmitPool[SPoolHead].OVStruct,
            NULL
        );

        SPoolHead = (SPoolHead + 1) & (POOL_SIZE - 1);
    }

    // Zero-copy chunked send so.. data must remain valid until OnComplete() fires,
    // which happens inside the IOCP thread after the kernel has sent every chunk.
    // OnComplete is optional tho.
    // Single-chunk frames (data_size <= MTU) are sent as ChunkEnd directly,
    // which the receiver already handles correctly.
    void ChunkedSend(CHAR* data, int data_size, std::function<void()> OnComplete)
    {
        const uint32_t ChunkCount = (uint32_t)((data_size + (int)MTU - 1) / (int)MTU);

        const uint32_t TokenIndex =
            STokenHead.fetch_add(1, std::memory_order_relaxed) & (STOKEN_POOL_SIZE - 1);
        OmniNet::FrameCompletionToken& Token = STokenPool[TokenIndex];

        while (Token.PendingChunks.load(std::memory_order_acquire) != 0) {
            _mm_pause();
        }
        Token.OnComplete = std::move(OnComplete);
        Token.PendingChunks.store(ChunkCount, std::memory_order_release);

        // Reserves all inflight slots at once before issuing any WSASend calls.
        while (true) {
            uint32_t cur = SPoolInflight.load(std::memory_order_acquire);
            if (cur + ChunkCount <= POOL_SIZE) {
                if (SPoolInflight.compare_exchange_weak(
                        cur, cur + ChunkCount, std::memory_order_acq_rel, std::memory_order_acquire
                    ))
                    break;
            } else {
                _mm_pause();
            }
        }

        for (uint32_t i = 0; i < ChunkCount; ++i) {
            const int offset = (int)(i * MTU);
            const int chunkLen = (i + 1 < ChunkCount) ? (int)MTU : (data_size - offset);

            OmniNet::PacketType type;
            if (ChunkCount == 1)
                type = OmniNet::ChunkEnd;
            else if (i == 0)
                type = OmniNet::ChunkStart;
            else if (i + 1 < ChunkCount)
                type = OmniNet::ChunkData;
            else
                type = OmniNet::ChunkEnd;

            CHeaderPool[SPoolHead].PacketType = type;
            CHeaderPool[SPoolHead].Target = 0;
            TransmitPool[SPoolHead].TransmitBuffer[0].buf = data + offset;
            TransmitPool[SPoolHead].TransmitBuffer[0].len = (ULONG)chunkLen;
            TransmitPool[SPoolHead].Token = &Token;

            WSASend(
                SocketR,
                TransmitPool[SPoolHead].TransmitBuffer,
                2,
                NULL,
                0,
                &TransmitPool[SPoolHead].OVStruct,
                NULL
            );

            SPoolHead = (SPoolHead + 1) & (POOL_SIZE - 1);
        }
    }
};

#endif // SESSIONHANDLER_H
