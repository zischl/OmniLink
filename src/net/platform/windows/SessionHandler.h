#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H

#pragma once
#include "OmniConfig.h"
#include "OmniLogger.h"
#include "OmniNetContext.h"
#include "OmniTypes.h"
#include "SessionTypes.h"
#include "SubStream.h"

#include <iphlpapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

    // Session Identity & Context passed down to packet handler
    uint8_t UniqueKey = 0;
    OmniNet::SessionPacketContext SessionCtx;

    // Usual send queue and header pools
    static constexpr uint32_t POOL_SIZE = 2048;

    uint32_t SPoolHead = 0;
    OmniNet::SEND_BUF TransmitPool[POOL_SIZE];
    OmniNet::OmniHeader CHeaderPool[POOL_SIZE];

    // Tracks how many WSASend OVERLAPPED ops are still owned by the kernel.
    std::atomic<uint32_t> SPoolInflight{0};
    std::atomic<uint32_t> STokenHead{0};
    static constexpr uint32_t STOKEN_POOL_SIZE = 32;
    OmniNet::FrameCompletionToken STokenPool[STOKEN_POOL_SIZE];

    // Lonely Recv pool, don't care about headers, head is built in
    OmniNet::IOContextChunkPool<OmniNet::RECV_BUF, POOL_SIZE, MTU> RecvPool;

    // UDP Sub Streams, still working on it
    static constexpr uint32_t MaxSubStreams = 8;
    OmniNetSubStream SubStreams[MaxSubStreams];

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
        OmniNetContext::PostWSARecv(
            SocketR,
            &RecvPool.ContextPool[RecvPool.PoolHead].TransmitBuffer,
            &RecvPool.ContextPool[RecvPool.PoolHead].OVStruct
        );
    }

  public:
    OmniNetSession(PCSTR Local_IP, PCSTR IP, unsigned short port, void* Context, uint8_t SUniqueKey)
        : UniqueKey(SUniqueKey), SessionCtx{Context, SUniqueKey}
    {
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
        SessionState = false;

        if (SocketR != INVALID_SOCKET) {
            CancelIoEx(reinterpret_cast<HANDLE>(SocketR), NULL);
            closesocket(SocketR);
            SocketR = INVALID_SOCKET;
        }

        if (IOCPHandle) {
            PostQueuedCompletionStatus(IOCPHandle, 0, 0, nullptr);
        }

        if (WorkerThread.joinable()) {
            WorkerThread.join();
        }

        if (IOCPHandle) {
            CloseHandle(IOCPHandle);
            IOCPHandle = NULL;
        }
    }

    template <void (*PacketHandlerFn)(char*, uint32_t, uint8_t, void*)>
    void SessionStart(void* Context)
    {
        WorkerThread = std::thread([this, Context]() {
            SessionCtx.UserContext = Context;

            while (true) {
                DWORD BufferSize = 0;
                ULONG_PTR EventKey = 0;
                OVERLAPPED* OVStruct = nullptr;

                BOOL CompletionStatus = GetQueuedCompletionStatus(
                    IOCPHandle, &BufferSize, &EventKey, &OVStruct, INFINITE
                );

                if (OVStruct == nullptr) [[unlikely]] {
                    break;
                }

                if (!CompletionStatus) [[unlikely]] {
                    OmniNet::IOContextTag* Tag = reinterpret_cast<OmniNet::IOContextTag*>(OVStruct);
                    if (Tag->Type == OmniNet::OP_SEND) {
                        OmniNet::SEND_BUF* SendBuf = reinterpret_cast<OmniNet::SEND_BUF*>(OVStruct);
                        if (SendBuf->InflightCounter) {
                            SendBuf->InflightCounter->fetch_sub(1, std::memory_order_release);
                        }
                    }
                    continue;
                }

                if (EventKey != 0) {
                    OmniNetSubStream* SubStream = reinterpret_cast<OmniNetSubStream*>(EventKey);
                    if (SubStream->SubStreamState())
                        SubStream->HandleCompletion(OVStruct, BufferSize);
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
                                &SessionCtx
                            );
                        }
                        RecvPool.ResetChunk();
                        break;
                    default:
                        PacketHandlerFn(
                            Buffer->TransmitBuffer.buf, BufferSize, BufferHeader, &SessionCtx
                        );
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
                        if (SendBuf->Token->OnComplete) {
                            SendBuf->Token->OnComplete(SendBuf->Token->Arg1, SendBuf->Token->Arg2);
                        }
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

    // Opens a sub stream endpoint. Binds to a local ephemeral port and registers with IOCP.
    // Needs an external data pointer for recv, wires it up and starts receive operations.
    // Literally built for zero copy output targets
    // Returns a stable pointer into SubStreams container
    // The stable pointer itself is returned as the completion key in IOCP
    OmniNetSubStream* OpenSubStream(
        char* Data = nullptr,
        uint32_t DataSize = 0,
        uint32_t NumSlots = 0,
        void (*OnSlotComplete)(void* ctx, uint32_t slot, uint32_t size) = nullptr,
        void* SlotCompleteCtx = nullptr
    )
    {
        for (uint32_t i = 0; i < MaxSubStreams; ++i) {
            if (!SubStreams[i].SubStreamState()) {
                if (!SubStreams[i].Init(
                        LocalIPString,
                        IOCPHandle,
                        Data,
                        DataSize,
                        NumSlots,
                        OnSlotComplete,
                        SlotCompleteCtx
                    ))
                    return nullptr;
                return &SubStreams[i];
            }
        }
        return nullptr;
    }

    void CloseSubStream(OmniNetSubStream* SubStream)
    {
        if (SubStream)
            SubStream->Close();
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
    bool ChunkedSend(
        CHAR* Data,
        int DataSize,
        void (*OnComplete)(void*, size_t) = nullptr,
        void* Arg1 = nullptr,
        size_t Arg2 = 0
    )
    {
        const uint32_t ChunkCount = (uint32_t)((DataSize + (int)MTU - 1) / (int)MTU);

        const uint32_t TokenIndex =
            STokenHead.fetch_add(1, std::memory_order_relaxed) & (STOKEN_POOL_SIZE - 1);
        OmniNet::FrameCompletionToken& Token = STokenPool[TokenIndex];

        if (Token.PendingChunks.load(std::memory_order_acquire) != 0) {
            if (OnComplete)
                OnComplete(Arg1, Arg2);
            return false;
        }

        // Reserves all inflight slots at once.
        while (true) {
            uint32_t ActiveSPoolUsage = SPoolInflight.load(std::memory_order_acquire);
            if (ActiveSPoolUsage + ChunkCount <= POOL_SIZE) {
                if (SPoolInflight.compare_exchange_weak(
                        ActiveSPoolUsage,
                        ActiveSPoolUsage + ChunkCount,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    ))
                    break;
            } else {
                if (OnComplete)
                    OnComplete(Arg1, Arg2);
                return false;
            }
        }

        Token.OnComplete = OnComplete;
        Token.Arg1 = Arg1;
        Token.Arg2 = Arg2;
        Token.PendingChunks.store(ChunkCount, std::memory_order_release);

        for (uint32_t i = 0; i < ChunkCount; ++i) {
            const int offset = (int)(i * MTU);
            const int chunkLen = (i + 1 < ChunkCount) ? (int)MTU : (DataSize - offset);

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
            TransmitPool[SPoolHead].TransmitBuffer[0].buf = Data + offset;
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
        return true;
    }
};

#endif // SESSIONHANDLER_H
