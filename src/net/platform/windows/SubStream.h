#ifndef SUBSTREAM_H
#define SUBSTREAM_H

#pragma once

#include "SessionTypes.h"

#include <winsock2.h>

#include <atomic>

class OmniNetSubStream
{
  public:
    OmniNetSubStream() = default;

    OmniNetSubStream(const OmniNetSubStream&) = delete;
    OmniNetSubStream& operator=(const OmniNetSubStream&) = delete;

    // Initializes the socket, binds it locally to an ephemeral port, registers with IOCP,
    // presets the send pool, and wires recv pool up to posts receive buffers if given.
    bool Init(
        PCSTR LocalIP,
        HANDLE SharedIOCP,
        char* DataBase = nullptr,
        uint32_t DataSize = 0,
        uint32_t NumSlots = 0,
        void (*OnSlotComplete)(void* ctx, uint32_t slot, uint32_t size) = nullptr,
        void* SlotCompleteCtx = nullptr
    );

    // Connects the socket to the remote peer's sub stream port.
    bool Connect(PCSTR RemoteIP, uint16_t RemotePort);

    // Late binds an external recv pool after the sub-stream is already created and connected.
    bool BindRecvPool(
        char* Data,
        uint32_t DataSize,
        uint32_t NumSlots,
        void (*OnSlotComplete)(void* ctx, uint32_t slot, uint32_t size),
        void* Ctx
    );

    // Pretty much same as the main OmniNetSession's version.
    // Independent send pools to avoid contention on main session
    bool ChunkedSend(
        CHAR* Data,
        int DataSize,
        void (*OnComplete)(void*, size_t) = nullptr,
        void* Arg1 = nullptr,
        size_t Arg2 = 0
    );

    // Called by OmniNetSession's IOCP thread when completion key matches
    // (ULONG_PTR)this.. I mean that.. like.. this sub stream instance
    void HandleCompletion(OVERLAPPED* OV, DWORD BufferSize);

    // Socket and State gone bye bye !
    // U might wanna skip completions based on GetStreamState
    void Close();

    uint16_t GetLocalPort() const { return LocalPort; }

    bool SubStreamState() const { return Active; }

    ~OmniNetSubStream() { Close(); }

  private:
    SOCKET StreamSocket = INVALID_SOCKET;
    uint16_t LocalPort = 0;
    bool Active = false;

    // Recv Pool
    OmniNet::IOContextExternalChunkPool<OmniNet::RECV_BUF> RecvPool;

    // Send Pool
    static constexpr uint32_t SPOOL_SIZE = 2048;
    static constexpr uint32_t STOKEN_POOL_SIZE = 32;
    OmniNet::IOContextTransmitRing<OmniNet::SEND_BUF, OmniNet::OmniHeader, SPOOL_SIZE> SendPool;
    std::atomic<uint32_t> SPoolInflight{0};
    std::atomic<uint32_t> STokenHead{0};
    OmniNet::FrameCompletionToken STokenPool[STOKEN_POOL_SIZE];

    void PostNextRecv();
    void PreSetSendPool();
};

#endif // SUBSTREAM_H
