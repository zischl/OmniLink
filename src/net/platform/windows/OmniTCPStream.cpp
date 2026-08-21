#include "OmniTCPStream.h"
#include "OmniLogger.h"

#include <chrono>
#include <fstream>
#include <thread>

OmniTCPStream::OmniTCPStream(uint32_t StreamIDIn, StreamChannelState InitialState)
    : StreamID(StreamIDIn), ChannelState(static_cast<uint8_t>(InitialState))
{
}

OmniTCPStream::~OmniTCPStream()
{
    End();
}

bool OmniTCPStream::ConfigureSocket(SocketHandle Socket)
{
    if (Socket == InvalidSocket) {
        return false;
    }

    int NoDelay = 1;
    setsockopt(
        Socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&NoDelay), sizeof(NoDelay)
    );

    int BufSize = SOCKET_BUFFER_SIZE;
    setsockopt(
        Socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&BufSize), sizeof(BufSize)
    );
    setsockopt(
        Socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&BufSize), sizeof(BufSize)
    );

    return true;
}

void OmniTCPStream::CloseSocketHandle(SocketHandle& Socket)
{
    if (Socket != InvalidSocket) {
        shutdown(Socket, SD_BOTH);
        closesocket(Socket);
        Socket = InvalidSocket;
    }
}

bool OmniTCPStream::StartServer(uint16_t Port)
{
    End();
    CancelRequested.store(false);

    ListenSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (ListenSocket == InvalidSocket) {
        Logger::log("Failed to create OmniTCPStream@[StreamID: {:d}] ListenSocket", StreamID);
        return false;
    }

    ConfigureSocket(ListenSocket);

    int Reuse = 1;
    setsockopt(
        ListenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&Reuse), sizeof(Reuse)
    );

    sockaddr_in ServerAddr{};
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(Port);

    if (bind(ListenSocket, reinterpret_cast<sockaddr*>(&ServerAddr), sizeof(ServerAddr)) ==
        SOCKET_ERROR) {
        Logger::log("Bind failed on OmniTCPStream@[StreamID: {:d}] port {:d}", StreamID, Port);
        CloseSocketHandle(ListenSocket);
        return false;
    }

    if (listen(ListenSocket, 5) == SOCKET_ERROR) {
        Logger::log("Listen failed OmniTCPStream@[StreamID: {:d}]", StreamID);
        CloseSocketHandle(ListenSocket);
        return false;
    }

    sockaddr_in BoundAddr{};
    int AddrLen = sizeof(BoundAddr);
    if (getsockname(ListenSocket, reinterpret_cast<sockaddr*>(&BoundAddr), &AddrLen) == 0) {
        LocalPort = ntohs(BoundAddr.sin_port);
    } else {
        LocalPort = Port;
    }

    UpdateChannelState(StreamChannelState::SendOnly, true);
    Active.store(true);
    Logger::log(
        "Server listening on OmniTCPStream@[StreamID: {:d}] ephemeral port {:d}",
        StreamID,
        LocalPort
    );
    return true;
}

bool OmniTCPStream::AcceptClient(uint32_t TimeoutMs)
{
    if (ListenSocket == InvalidSocket) {
        return false;
    }

    fd_set ReadSet;
    FD_ZERO(&ReadSet);
    FD_SET(ListenSocket, &ReadSet);

    timeval TV{};
    TV.tv_sec = TimeoutMs / 1000;
    TV.tv_usec = (TimeoutMs % 1000) * 1000;

    int SelectRes = select(0, &ReadSet, nullptr, nullptr, (TimeoutMs > 0) ? &TV : nullptr);
    if (SelectRes <= 0 || CancelRequested.load()) {
        return false;
    }

    sockaddr_in ClientAddr{};
    int ClientLen = sizeof(ClientAddr);
    ActiveSocket = accept(ListenSocket, reinterpret_cast<sockaddr*>(&ClientAddr), &ClientLen);
    if (ActiveSocket == InvalidSocket) {
        Logger::log("Accept failed on OmniTCPStream@[StreamID: {:d}]", StreamID);
        return false;
    }

    ConfigureSocket(ActiveSocket);

    char ClientIP[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &ClientAddr.sin_addr, ClientIP, sizeof(ClientIP));
    Logger::log(
        "Client connected from OmniTCPStream@[StreamID: {:d}] {:s}:{:d}",
        StreamID,
        ClientIP,
        ntohs(ClientAddr.sin_port)
    );
    return true;
}

bool OmniTCPStream::Connect(const std::string& RemoteIP, uint16_t RemotePort, uint32_t TimeoutMs)
{
    End();
    CancelRequested.store(false);

    ActiveSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (ActiveSocket == InvalidSocket) {
        Logger::log("Failed to create OmniTCPStream@[StreamID: {:d}] client socket", StreamID);
        return false;
    }

    ConfigureSocket(ActiveSocket);

    sockaddr_in TargetAddr{};
    TargetAddr.sin_family = AF_INET;
    TargetAddr.sin_port = htons(RemotePort);
    if (inet_pton(AF_INET, RemoteIP.c_str(), &TargetAddr.sin_addr) <= 0) {
        Logger::log(
            "Invalid IP address on OmniTCPStream@[StreamID: {:d}] {:s}", StreamID, RemoteIP.c_str()
        );
        CloseSocketHandle(ActiveSocket);
        return false;
    }

    u_long NonBlocking = 1;
    ioctlsocket(ActiveSocket, FIONBIO, &NonBlocking);

    int ConnectRes =
        connect(ActiveSocket, reinterpret_cast<sockaddr*>(&TargetAddr), sizeof(TargetAddr));
    if (ConnectRes == SOCKET_ERROR) {
        int Err = WSAGetLastError();
        if (Err != WSAEWOULDBLOCK) {
            CloseSocketHandle(ActiveSocket);
            return false;
        }

        fd_set WriteSet;
        FD_ZERO(&WriteSet);
        FD_SET(ActiveSocket, &WriteSet);

        timeval TV{};
        TV.tv_sec = TimeoutMs / 1000;
        TV.tv_usec = (TimeoutMs % 1000) * 1000;

        int SelectRes = select(0, nullptr, &WriteSet, nullptr, &TV);
        if (SelectRes <= 0) {
            Logger::log(
                "Connection timeout to OmniTCPStream@[StreamID: {:d}] {:s}:{:d}",
                StreamID,
                RemoteIP.c_str(),
                RemotePort
            );
            CloseSocketHandle(ActiveSocket);
            return false;
        }
    }

    NonBlocking = 0;
    ioctlsocket(ActiveSocket, FIONBIO, &NonBlocking);

    UpdateChannelState(StreamChannelState::RecvOnly, true);
    Active.store(true);
    Logger::log(
        "Connected to OmniTCPStream@[StreamID: {:d}] {:s}:{:d}",
        StreamID,
        RemoteIP.c_str(),
        RemotePort
    );
    return true;
}

bool OmniTCPStream::StreamBuffer(const uint8_t* Data, size_t TotalSize, ProgressCallback OnProgress)
{
    if (!Data || TotalSize == 0 || ActiveSocket == InvalidSocket) {
        return false;
    }

    size_t BytesSent = 0;
    while (BytesSent < TotalSize && !CancelRequested.load()) {
        size_t ChunkToSend = (std::min)(CHUNK_SIZE, TotalSize - BytesSent);
        int Sent = send(
            ActiveSocket,
            reinterpret_cast<const char*>(Data + BytesSent),
            static_cast<int>(ChunkToSend),
            0
        );

        if (Sent <= 0) {
            Logger::log(
                "StreamBuffer OmniTCPStream@[StreamID: {:d}] send failed (Err: {:d})",
                StreamID,
                WSAGetLastError()
            );
            return false;
        }

        BytesSent += Sent;
        if (OnProgress) {
            OnProgress(BytesSent, TotalSize);
        }
    }

    return BytesSent == TotalSize;
}

bool OmniTCPStream::StreamFile(const std::wstring& FilePath, ProgressCallback OnProgress)
{
    if (ActiveSocket == InvalidSocket) {
        return false;
    }

    HANDLE FileHandle = CreateFileW(
        FilePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (FileHandle == INVALID_HANDLE_VALUE) {
        Logger::log("Failed to open file for OmniTCPStream@[StreamID: {:d}]", StreamID);
        return false;
    }

    LARGE_INTEGER FileSize{};
    if (!GetFileSizeEx(FileHandle, &FileSize)) {
        CloseHandle(FileHandle);
        return false;
    }

    size_t TotalSize = static_cast<size_t>(FileSize.QuadPart);
    size_t BytesSent = 0;
    std::vector<uint8_t> Buffer(CHUNK_SIZE);

    while (BytesSent < TotalSize && !CancelRequested.load()) {
        DWORD BytesToRead = static_cast<DWORD>((std::min)(CHUNK_SIZE, TotalSize - BytesSent));
        DWORD BytesRead = 0;

        if (!ReadFile(FileHandle, Buffer.data(), BytesToRead, &BytesRead, nullptr) ||
            BytesRead == 0) {
            break;
        }

        int Sent = send(ActiveSocket, reinterpret_cast<const char*>(Buffer.data()), BytesRead, 0);
        if (Sent <= 0) {
            CloseHandle(FileHandle);
            return false;
        }

        BytesSent += Sent;
        if (OnProgress) {
            OnProgress(BytesSent, TotalSize);
        }
    }

    CloseHandle(FileHandle);
    return BytesSent == TotalSize;
}

bool OmniTCPStream::ReceiveToBuffer(
    std::vector<uint8_t>& OutBuffer, size_t ExpectedSize, ProgressCallback OnProgress
)
{
    if (ActiveSocket == InvalidSocket || ExpectedSize == 0) {
        return false;
    }

    OutBuffer.resize(ExpectedSize);
    size_t BytesReceived = 0;

    while (BytesReceived < ExpectedSize && !CancelRequested.load()) {
        size_t ChunkToRecv = (std::min)(CHUNK_SIZE, ExpectedSize - BytesReceived);
        int RecvBytes = recv(
            ActiveSocket,
            reinterpret_cast<char*>(OutBuffer.data() + BytesReceived),
            static_cast<int>(ChunkToRecv),
            0
        );

        if (RecvBytes <= 0) {
            Logger::log(
                "ReceiveToBuffer OmniTCPStream@[StreamID: {:d}] recv failed (Err: {:d})",
                StreamID,
                WSAGetLastError()
            );
            return false;
        }

        BytesReceived += RecvBytes;
        if (OnProgress) {
            OnProgress(BytesReceived, ExpectedSize);
        }
    }

    return BytesReceived == ExpectedSize;
}

bool OmniTCPStream::ReceiveToFile(
    const std::wstring& DestFilePath, size_t ExpectedSize, ProgressCallback OnProgress
)
{
    if (ActiveSocket == InvalidSocket) {
        return false;
    }

    HANDLE FileHandle = CreateFileW(
        DestFilePath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (FileHandle == INVALID_HANDLE_VALUE) {
        Logger::log("Failed to create output file on OmniTCPStream@[StreamID: {:d}]", StreamID);
        return false;
    }

    size_t BytesReceived = 0;
    std::vector<uint8_t> Buffer(CHUNK_SIZE);

    while (BytesReceived < ExpectedSize && !CancelRequested.load()) {
        size_t ChunkToRecv = (std::min)(CHUNK_SIZE, ExpectedSize - BytesReceived);
        int RecvBytes = recv(
            ActiveSocket, reinterpret_cast<char*>(Buffer.data()), static_cast<int>(ChunkToRecv), 0
        );

        if (RecvBytes <= 0) {
            CloseHandle(FileHandle);
            return false;
        }

        DWORD BytesWritten = 0;
        if (!WriteFile(FileHandle, Buffer.data(), RecvBytes, &BytesWritten, nullptr) ||
            BytesWritten != static_cast<DWORD>(RecvBytes)) {
            CloseHandle(FileHandle);
            return false;
        }

        BytesReceived += RecvBytes;
        if (OnProgress) {
            OnProgress(BytesReceived, ExpectedSize);
        }
    }

    CloseHandle(FileHandle);
    return BytesReceived == ExpectedSize;
}

bool OmniTCPStream::ReceiveChunkStream(
    ChunkReceiveCallback OnChunk, size_t ExpectedSize, ProgressCallback OnProgress
)
{
    if (ActiveSocket == InvalidSocket || !OnChunk) {
        return false;
    }

    size_t BytesReceived = 0;
    std::vector<uint8_t> Buffer(CHUNK_SIZE);

    while (BytesReceived < ExpectedSize && !CancelRequested.load()) {
        size_t ChunkToRecv = (std::min)(CHUNK_SIZE, ExpectedSize - BytesReceived);
        int RecvBytes = recv(
            ActiveSocket, reinterpret_cast<char*>(Buffer.data()), static_cast<int>(ChunkToRecv), 0
        );

        if (RecvBytes <= 0) {
            return false;
        }

        if (!OnChunk(Buffer.data(), static_cast<size_t>(RecvBytes))) {
            return false;
        }

        BytesReceived += RecvBytes;
        if (OnProgress) {
            OnProgress(BytesReceived, ExpectedSize);
        }
    }

    return BytesReceived == ExpectedSize;
}

void OmniTCPStream::SetChannelState(StreamChannelState NewState)
{
    ChannelState.store(static_cast<uint8_t>(NewState), std::memory_order_release);
}

void OmniTCPStream::UpdateChannelState(StreamChannelState Flag, bool Enable)
{
    if (Enable) {
        ChannelState.fetch_or(static_cast<uint8_t>(Flag), std::memory_order_acq_rel);
    } else {
        ChannelState.fetch_and(~static_cast<uint8_t>(Flag), std::memory_order_acq_rel);
    }
}

void OmniTCPStream::Cancel()
{
    CancelRequested.store(true);
    CloseSocketHandle(ActiveSocket);
}

void OmniTCPStream::End()
{
    Active.store(false);
    Cancel();
    CloseSocketHandle(ListenSocket);
    SetChannelState(StreamChannelState::Inactive);
    LocalPort = 0;
}
