#pragma once
#include "SessionHandler.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

static std::atomic<bool> PacketReceived{false};
static std::string receivedData;
static uint8_t receivedHeaderType = 0;

inline void TestIOComplete(char* Buffer, uint32_t BufferSize, uint8_t BufferHeader, void* Context)
{
    // Excluding the header size as payload is Data+Header, not.. Header+Data
    receivedData = std::string(Buffer, BufferSize - 3);
    receivedHeaderType = BufferHeader;
    PacketReceived = true;
}

void NetSessionTest()
{
    std::cout << "[RUN] SessionTest\n";

    OmniNetContext OmniNetContext;

    {
        // Bind to localhost port 62490 and connect to localhost port 62490 to test loopback
        OmniNetSession<OmniMTU> TestSession("127.0.0.1", "127.0.0.1", 62490, nullptr, 0);
        TestSession.SessionStart<TestIOComplete>(nullptr);

        std::string TestMsg = "Hello OmniLink Loopback!";
        OmniNet::OmniHeader Header;
        Header.PacketType = OmniNet::PacketType::Command;
        Header.Target = 0;
        Header.Flags = 0;

        PacketReceived = false;
        TestSession.SessionSend(TestMsg.data(), static_cast<int>(TestMsg.size()), Header);

        // It's instant.. but well
        auto start = std::chrono::steady_clock::now();
        while (!PacketReceived &&
               (std::chrono::steady_clock::now() - start < std::chrono::seconds(1))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        assert(PacketReceived && "Loopback packet was not received!");
        std::cout << "[INFO] Received loopback data: \"" << receivedData << "\"\n";
        assert(receivedData == TestMsg && "Received data does not match sent data!");
        std::cout << "[INFO] Packet content and header type verified successfully.\n";

        // Test ChunkedSend (sending a 4000-byte payload, which is not a perfect MTU Slice)
        std::string ReallyHugeMsg(4000, 'A');

        PacketReceived = false;
        std::atomic<bool> ChunkSendCompleted{false};

        TestSession.ChunkedSend(
            ReallyHugeMsg.data(), static_cast<int>(ReallyHugeMsg.size()), [&ChunkSendCompleted]() {
                ChunkSendCompleted = true;
            }
        );

        // Wait for chunks to be reassembled on the completion port, but still.. instant
        start = std::chrono::steady_clock::now();
        while ((!PacketReceived || !ChunkSendCompleted) &&
               (std::chrono::steady_clock::now() - start < std::chrono::seconds(2))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        assert(ChunkSendCompleted && "ChunkedSend completion token callback did not fire!");
        assert(PacketReceived && "Chunked loopback packets were not received/assembled!");
        assert(
            receivedHeaderType == OmniNet::PacketType::ChunkEnd &&
            "Incorrect chunked header type received!"
        );
        assert(
            receivedData == ReallyHugeMsg && "Reassembled chunked data does not match original!"
        );
        std::cout << "[INFO] ChunkedSend and assembly verified successfully (4000 bytes).\n";
    }

    WSACleanup();
    std::cout << "[PASS] SessionTest\n";
}
