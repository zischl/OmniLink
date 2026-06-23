#include "OmniDiscovery.h"
#include "OmniLogger.h"
#include "asio/io_context.hpp"
#include <chrono>
#include <mutex>

// class connections {
//
// };
//
// void create_listener(asio::ip::tcp::socket& socket) {
//     uint32_t length;
//     std::vector<char> buffer;
//     while (true) {
//         asio::read(socket, asio::buffer(&length, sizeof(length)));
//         buffer.resize(length);
//         asio::read(socket, asio::buffer(buffer.data(), length));
//         std::cout << "Received: " << std::string(buffer.begin(), buffer.end()) << std::endl;
//     }
// }
//
// void start_listener(asio::io_context& io, const std::string& ip, unsigned short port) {
//     asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);
//     asio::ip::tcp::socket socket(io);
//     socket.connect(endpoint);
//     std::cout << "listener active!\n";
//     create_listener(socket);
// }
//
// void send(asio::ip::tcp::socket& socket) {
//     while (true) {
//         std::string msg;
//         std::cout << " : ";
//         std::cin >> msg;
//         if (msg == "exit") {
//             break;
//         }
//         uint32_t length = msg.size();
//         asio::write(socket, asio::buffer(&length, sizeof(length)));
//         asio::write(socket, asio::buffer(msg));
//
//     }
// }
//
// void start_sender(asio::io_context& io, unsigned short port) {
//     asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
//     asio::ip::tcp::socket socket(io);
//     acceptor.accept(socket);
//     std::cout << "Instance Connected!\n";
//     send(socket);
// }

OmniDiscovery::OmniDiscovery(const std::string& InstanceName, uint32_t _LocalIP, uint16_t port)
    : io_context(), socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port))
{
    instances[_LocalIP] = InstanceName;
    InstanceIP = _LocalIP;
    discovery_port = port;

    socket.set_option(asio::socket_base::reuse_address(true));
}

void OmniDiscovery::PopulateInstances(int Runtime)
{
    AwaitInstances(Runtime);
    Scan(Runtime);
}

std::unordered_map<uint32_t, std::string> OmniDiscovery::get()
{
    std::lock_guard<std::mutex> lock(mutex);
    return instances;
}

void OmniDiscovery::Scan(int runtime)
{
    if (!ScanState.load()) {

        ScanState.store(true);

        std::thread broadcaster([this, runtime]() {
            std::chrono::steady_clock::duration Runtime = std::chrono::seconds(runtime);

            std::chrono::time_point<std::chrono::steady_clock> Start =
                std::chrono::steady_clock::now();

            asio::ip::udp::socket socket(io_context);
            socket.open(asio::ip::udp::v4());
            socket.set_option(asio::socket_base::reuse_address(true));
            socket.set_option(asio::socket_base::broadcast(true));

            asio::ip::udp::endpoint broadcast_endpoint(
                asio::ip::make_address("255.255.255.255"), discovery_port
            );

            while ((std::chrono::steady_clock::now() - Start) <= Runtime) {

                OmniDiscoveryPacket DiscoveryPacket{
                    PayloadType::DiscoveryRequest, sizeof(OmniDiscoveryRequest)
                };

                std::memcpy(
                    DiscoveryPacket.Payload, OmniDiscoveryRequest, sizeof(OmniDiscoveryRequest)
                );

                memcpy(DiscoveryPacket.UUID, InstanceUUID.Bytes, sizeof(DiscoveryPacket.UUID));

                DiscoveryPacket.Liss = OmniLiss;

                socket.send_to(
                    asio::buffer(&DiscoveryPacket, sizeof(OmniDiscoveryPacket)), broadcast_endpoint
                );

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            ScanState.store(false);
        });

        broadcaster.detach();
    }
}

void OmniDiscovery::AwaitInstances()
{

    std::thread responder([this]() {
        asio::ip::udp::endpoint response_endpoint;

        while (state.load()) {

            HandleResponse(socket, response_endpoint);
        }
    });

    responder.detach();
}

void OmniDiscovery::AwaitInstances(int runtime)
{

    std::thread responder([this, runtime]() {
        std::chrono::steady_clock::duration Runtime =
            std::chrono::steady_clock::duration(std::chrono::seconds(runtime));

        std::chrono::time_point<std::chrono::steady_clock> Start = std::chrono::steady_clock::now();

        asio::ip::udp::endpoint response_endpoint;

        while ((std::chrono::steady_clock::now() - Start) <= Runtime) {

            HandleResponse(socket, response_endpoint);
        }
    });

    responder.detach();
}

void OmniDiscovery::SendCustomPayload(
    const std::string& TargetIPv4, uint16_t TargetPort, const OmniPayloadBase& Payload
)
{
    std::error_code ErrorCode;

    auto Address = asio::ip::make_address(TargetIPv4, ErrorCode);
    if (ErrorCode) {
        Logger::log("Invalid Target IP address: " + ErrorCode.message());
        return;
    }

    asio::ip::udp::endpoint TargetEndpoint(Address, TargetPort);
    OmniDiscoveryPacket Packet = OmniDiscoveryPacket::From(Payload);

    socket.send_to(
        asio::buffer(&Packet, sizeof(OmniDiscoveryPacket)), TargetEndpoint, 0, ErrorCode
    );

    if (ErrorCode) {
        Logger::log("Connection Request Died MidWay: " + ErrorCode.message());
    }
}

void OmniDiscovery::SendCustomPayload(
    uint32_t TargetIPv4, uint16_t TargetPort, const OmniPayloadBase& Payload
)
{
    std::error_code ErrorCode;

    asio::ip::udp::endpoint TargetEndpoint(asio::ip::address_v4(TargetIPv4), TargetPort);

    OmniDiscoveryPacket Packet = OmniDiscoveryPacket::From(Payload);

    socket.send_to(
        asio::buffer(&Packet, sizeof(OmniDiscoveryPacket)), TargetEndpoint, 0, ErrorCode
    );

    if (ErrorCode) {
        Logger::log("Connection Request Died MidWay: " + ErrorCode.message());
    }
}
