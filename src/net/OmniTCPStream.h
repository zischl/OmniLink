#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
constexpr SocketHandle InvalidSocket = -1;
#endif

enum class StreamChannelState : uint8_t {
    Inactive = 0,
    SendOnly = 1 << 0,
    RecvOnly = 1 << 1,
    Duplex = SendOnly | RecvOnly
};

class OmniTCPStream
{
  public:
    using ProgressCallback = std::function<void(size_t BytesTransferred, size_t TotalBytes)>;
    using ChunkReceiveCallback = std::function<bool(const uint8_t* Data, size_t ChunkSize)>;

    OmniTCPStream(
        uint32_t StreamID = 0, StreamChannelState InitialState = StreamChannelState::Inactive
    );
    ~OmniTCPStream();

    OmniTCPStream(const OmniTCPStream&) = delete;
    OmniTCPStream& operator=(const OmniTCPStream&) = delete;

    // Server operations
    bool StartServer(uint16_t Port = 0);
    bool AcceptClient(uint32_t TimeoutMs = 5000);

    // Client operations
    bool Connect(const std::string& RemoteIP, uint16_t RemotePort, uint32_t TimeoutMs = 5000);

    // Streaming functions
    bool StreamBuffer(const uint8_t* Data, size_t TotalSize, ProgressCallback OnProgress = nullptr);
    bool StreamFile(const std::wstring& FilePath, ProgressCallback OnProgress = nullptr);
    bool ReceiveToBuffer(
        std::vector<uint8_t>& OutBuffer, size_t ExpectedSize, ProgressCallback OnProgress = nullptr
    );
    bool ReceiveToFile(
        const std::wstring& DestFilePath, size_t ExpectedSize, ProgressCallback OnProgress = nullptr
    );
    bool ReceiveChunkStream(
        ChunkReceiveCallback OnChunk, size_t ExpectedSize, ProgressCallback OnProgress = nullptr
    );

    // State management
    void SetChannelState(StreamChannelState NewState);
    void UpdateChannelState(StreamChannelState Flag, bool Enable);
    void End();

    // Cancellation
    void Cancel();
    bool CancelRequestState() const { return CancelRequested.load(); }

    // Properties
    uint32_t GetStreamID() const { return StreamID; }
    uint16_t GetLocalPort() const { return LocalPort; }
    StreamChannelState GetChannelState() const
    {
        return static_cast<StreamChannelState>(ChannelState.load());
    }
    bool GetStreamState() const { return Active.load(); }

  private:
    uint32_t StreamID = 0;
    uint16_t LocalPort = 0;
    std::atomic<uint8_t> ChannelState{static_cast<uint8_t>(StreamChannelState::Inactive)};
    std::atomic<bool> Active{false};
    std::atomic<bool> CancelRequested{false};

    SocketHandle ListenSocket = InvalidSocket;
    SocketHandle ActiveSocket = InvalidSocket;

    static constexpr size_t CHUNK_SIZE = 64 * 1024;        // 64 KB streaming chunks
    static constexpr int SOCKET_BUFFER_SIZE = 1024 * 1024; // 1 MB send/recv buffer

    bool ConfigureSocket(SocketHandle Socket);
    void CloseSocketHandle(SocketHandle& Socket);
};
