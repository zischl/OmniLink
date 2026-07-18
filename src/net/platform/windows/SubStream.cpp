#include "SubStream.h"
#include "OmniConfig.h"
#include "OmniNetContext.h"

#include <ws2tcpip.h>

bool OmniNetSubStream::Init(
    PCSTR LocalIP,
    HANDLE SharedIOCP,
    char* RDataOutputPtr,
    uint32_t DataSize,
    uint32_t NumSlots,
    void (*OnSlotComplete)(void* ctx, uint32_t slot, uint32_t size),
    void* SlotCompleteCtx
)
{
    SendPool.PoolHead = 0;
    SPoolInflight.store(0, std::memory_order_relaxed);
    STokenHead.store(0, std::memory_order_relaxed);

    StreamSocket = OmniNetContext::CreateSocket();
    if (StreamSocket == INVALID_SOCKET)
        return false;

    if (!OmniNetContext::BindReceiver(LocalIP, 0, StreamSocket)) {
        closesocket(StreamSocket);
        StreamSocket = INVALID_SOCKET;
        return false;
    }

    sockaddr_in SocketConfig{};
    int SocketConfigLen = sizeof(SocketConfig);
    getsockname(StreamSocket, reinterpret_cast<sockaddr*>(&SocketConfig), &SocketConfigLen);
    LocalPort = ntohs(SocketConfig.sin_port);

    PreSetSendPool();

    if (RDataOutputPtr) {
        RecvPool.Init(RDataOutputPtr, DataSize, NumSlots, OnSlotComplete, SlotCompleteCtx);
    } else {
        RecvPool.Reset();
    }

    if (!OmniNetContext::BindIOCP(SharedIOCP, StreamSocket, (ULONG_PTR)this)) {
        closesocket(StreamSocket);
        StreamSocket = INVALID_SOCKET;
        return false;
    }

    if (RDataOutputPtr) {
        PostNextRecv();
    }

    Active = true;
    return true;
}

bool OmniNetSubStream::Connect(PCSTR RemoteIP, uint16_t RemotePort)
{
    if (StreamSocket == INVALID_SOCKET)
        return false;

    const sockaddr_in Remote = OmniNetContext::CreateAddress(RemoteIP, RemotePort);
    if (!OmniNetContext::ConnectSesssion(Remote, StreamSocket))
        return false;

    return true;
}

// Pretty much a copy from the main session :]
bool OmniNetSubStream::ChunkedSend(
    CHAR* Data, int DataSize, void (*OnComplete)(void*, size_t), void* Arg1, size_t Arg2
)
{
    const uint32_t ChunkCount = (uint32_t)((DataSize + (int)OmniMTU - 1) / (int)OmniMTU);

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
        if (ActiveSPoolUsage + ChunkCount <= SPOOL_SIZE) {
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
        const int offset = (int)(i * OmniMTU);
        const int chunkLen = (i + 1 < ChunkCount) ? (int)OmniMTU : (DataSize - offset);

        OmniNet::PacketType type;
        if (ChunkCount == 1)
            type = OmniNet::ChunkEnd;
        else if (i == 0)
            type = OmniNet::ChunkStart;
        else if (i + 1 < ChunkCount)
            type = OmniNet::ChunkData;
        else
            type = OmniNet::ChunkEnd;

        const uint32_t head = SendPool.PoolHead;

        SendPool.HeaderPool[head].PacketType = type;
        SendPool.HeaderPool[head].Target = 0;
        SendPool.ContextPool[head].TransmitBuffer[0].buf = Data + offset;
        SendPool.ContextPool[head].TransmitBuffer[0].len = (ULONG)chunkLen;
        SendPool.ContextPool[head].Token = &Token;

        WSASend(
            StreamSocket,
            SendPool.ContextPool[head].TransmitBuffer,
            2,
            NULL,
            0,
            &SendPool.ContextPool[head].OVStruct,
            NULL
        );

        SendPool.PushChunk();
    }
    return true;
}

void OmniNetSubStream::HandleCompletion(OVERLAPPED* OV, DWORD BufferSize)
{
    OmniNet::IOContextTag* Tag = reinterpret_cast<OmniNet::IOContextTag*>(OV);

    if (Tag->Type == OmniNet::OP_SEND) {
        OmniNet::SEND_BUF* SBuffer = reinterpret_cast<OmniNet::SEND_BUF*>(OV);
        if (SBuffer->Token &&
            SBuffer->Token->PendingChunks.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            if (SBuffer->Token->OnComplete) {
                SBuffer->Token->OnComplete(SBuffer->Token->Arg1, SBuffer->Token->Arg2);
            }
        }
        if (SBuffer->InflightCounter)
            SBuffer->InflightCounter->fetch_sub(1, std::memory_order_release);
        return;
    }

    const char* RBuffer = RecvPool.ContextPool[RecvPool.PoolHead].TransmitBuffer.buf;
    const uint8_t PType = *(RBuffer + BufferSize - 3);

    switch (PType) {
    case OmniNet::ChunkStart:
    case OmniNet::ChunkData:
        RecvPool.PushChunk();
        break;

    case OmniNet::ChunkEnd:
        RecvPool.PushFinalChunk(BufferSize);
        break;
    }

    PostNextRecv();
}

void OmniNetSubStream::Close()
{
    Active = false;
    if (StreamSocket != INVALID_SOCKET) {
        closesocket(StreamSocket);
        StreamSocket = INVALID_SOCKET;
    }
    LocalPort = 0;
    RecvPool.Reset();
    SendPool.PoolHead = 0;
    SPoolInflight.store(0, std::memory_order_relaxed);
    STokenHead.store(0, std::memory_order_relaxed);
}

void OmniNetSubStream::PostNextRecv()
{
    OmniNetContext::PostWSARecv(
        StreamSocket,
        &RecvPool.ContextPool[RecvPool.PoolHead].TransmitBuffer,
        &RecvPool.ContextPool[RecvPool.PoolHead].OVStruct
    );
}

void OmniNetSubStream::PreSetSendPool()
{
    for (uint32_t i = 0; i < SPOOL_SIZE; ++i) {
        SendPool.ContextPool[i].OVStruct = {};
        SendPool.ContextPool[i].Type = OmniNet::OP_SEND;
        SendPool.ContextPool[i].TransmitBuffer[0].len = OmniMTU;
        SendPool.ContextPool[i].TransmitBuffer[1].len = OmniHeaderSize;
        SendPool.ContextPool[i].TransmitBuffer[1].buf =
            reinterpret_cast<CHAR*>(&SendPool.HeaderPool[i]);
        SendPool.ContextPool[i].InflightCounter = &SPoolInflight;
    }
}
