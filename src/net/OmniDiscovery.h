#ifndef OMNIDISCOVERY_H
#define OMNIDISCOVERY_H

#include <cstdint>
#include <string>
#pragma once

#include <asio.hpp>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>

// U might wonder what or who Liss is or why even, but i will be taking that to my grave it seems.
constexpr uint32_t OmniLiss = 0x4F4D4E49;
constexpr char OmniDiscoveryRequest[32] = "OmniLink Liss Where";
constexpr char OmniDiscoveryResponse[32] = "OmniLink Liss Who";

enum class PayloadType : uint8_t {
    DiscoveryRequest = 0x01,
    DiscoveryResponse = 0x02,
    ConnectRequest = 0x03,
    ConnectResponse = 0x04
};

#pragma pack(push, 1)
struct OmniDiscoveryPacket
{
    uint32_t Liss = 0x4F4D4E49;
    PayloadType Type;
    uint16_t PayloadLen = 0;
    char Payload[32] = {};
};
#pragma pack(pop)

class OmniDiscovery
{
  protected:
    std::unordered_map<uint32_t, std::string> instances;
    std::mutex mutex;
    std::atomic_bool state{true};

  public:
    OmniDiscovery(const std::string& InstanceName, uint32_t _LocalIP, uint16_t port);

    void PopulateInstances(int Runtime);

    std::unordered_map<uint32_t, std::string> get();

    /// <summary>
    /// Send out broadcast requests. Note that AwaitInstances should be running on
    /// the other device for Scan to work.
    /// </summary>
    void Scan(int runtime);
    std::atomic_bool ScanState{false};

    /// <summary>
    /// Await for new instance requests or responses to current device's requests
    /// on a blocking wait. Use EndAwait to stop the running.
    /// </summary>
    void AwaitInstances();

    /// <summary>
    /// Same as AwaitInstances but this one runs for a set duration as runtime
    /// (seconds).
    /// </summary>
    void AwaitInstances(int runtime);

    /// <summary>
    /// Same as AwaitInstances but this one can be used to add callbacks on
    /// instance found event. Usage ex : Instances->AwaitInstances([]() {
    /// dosomthing(); });
    /// </summary>
    template <typename Type> void AwaitInstances(Type&& Callback)
    {
        std::thread responder([this, Callback]() {
            asio::ip::udp::socket socket(
                io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), discovery_port));

            OmniDiscoveryPacket packet;
            asio::ip::udp::endpoint ResponseEndpoint;

            while (state.load()) {

                size_t msg_len =
                    socket.receive_from(asio::buffer(&packet, sizeof(packet)), ResponseEndpoint);

                if (msg_len >= sizeof(OmniDiscoveryPacket) && packet.Liss == 0x4F4D4E49) {
                    switch (packet.Type) {
                    case PayloadType::DiscoveryRequest:

                        if (std::memcmp(packet.Payload,
                                        OmniDiscoveryRequest,
                                        sizeof(OmniDiscoveryRequest)) == 0) {
                            ResponseEndpoint.port(discovery_port);

                            OmniDiscoveryPacket ResponsePacket;
                            ResponsePacket.Type = PayloadType::DiscoveryResponse;
                            std::memcpy(ResponsePacket.Payload,
                                        OmniDiscoveryResponse,
                                        sizeof(OmniDiscoveryResponse));
                            ResponsePacket.PayloadLen = sizeof(OmniDiscoveryResponse);

                            socket.send_to(asio::buffer(&ResponsePacket, sizeof(ResponsePacket)),
                                           ResponseEndpoint);

                            uint32_t Addr = ResponseEndpoint.address().to_v4().to_uint();
                            std::lock_guard<std::mutex> lock(mutex);
                            if (instances.find(Addr) == instances.end()) {
                                instances[Addr] = ResponseEndpoint.address().to_string();
                                std::cout << "Instance Found At: " << ResponseEndpoint.address()
                                          << " : " << ResponseEndpoint.port() << " "
                                          << socket.local_endpoint().port() << "\n";

                                Callback();
                            }
                        }

                        break;

                    case PayloadType::DiscoveryResponse:

                        if (std::memcmp(packet.Payload,
                                        OmniDiscoveryResponse,
                                        sizeof(OmniDiscoveryResponse)) == 0) {
                            uint32_t ip_addr = ResponseEndpoint.address().to_v4().to_uint();
                            std::lock_guard<std::mutex> lock(mutex);
                            if (instances.find(ip_addr) == instances.end()) {
                                instances[ip_addr] = ResponseEndpoint.address().to_string();
                                std::cout << "Instance Found At: " << ResponseEndpoint.address()
                                          << " : " << ResponseEndpoint.port() << " "
                                          << socket.local_endpoint().port() << "\n";

                                Callback();
                            }
                        }

                        break;

                    case PayloadType::ConnectResponse:
                        break;

                    case PayloadType::ConnectRequest:
                        break;

                    default:
                        break;
                    }
                }
            }
        });

        responder.detach();
    }

    /// <summary>
    /// Used to end AwaitInstances if not run with the parameter runtime.
    /// </summary>
    inline void EndAwait() { state.store(false); }

  private:
    unsigned short discovery_port;

    asio::io_context io_context;

    // Basic response handling using the OmniPacket structure.
    inline void HandleResponse(asio::ip::udp::socket& socket,
                               asio::ip::udp::endpoint& response_endpoint)
    {

        OmniDiscoveryPacket packet;
        asio::ip::udp::endpoint ResponseEndpoint;

        size_t msg_len =
            socket.receive_from(asio::buffer(&packet, sizeof(packet)), ResponseEndpoint);

        if (msg_len >= sizeof(OmniDiscoveryPacket) && packet.Liss == 0x4F4D4E49) {
            switch (packet.Type) {
            case PayloadType::DiscoveryRequest:

                if (std::memcmp(
                        packet.Payload, OmniDiscoveryRequest, sizeof(OmniDiscoveryRequest)) == 0) {
                    ResponseEndpoint.port(discovery_port);

                    OmniDiscoveryPacket ResponsePacket;
                    ResponsePacket.Type = PayloadType::DiscoveryResponse;
                    std::memcpy(ResponsePacket.Payload,
                                OmniDiscoveryResponse,
                                sizeof(OmniDiscoveryResponse));
                    ResponsePacket.PayloadLen = sizeof(OmniDiscoveryResponse);

                    socket.send_to(asio::buffer(&ResponsePacket, sizeof(ResponsePacket)),
                                   ResponseEndpoint);

                    uint32_t Addr = ResponseEndpoint.address().to_v4().to_uint();
                    std::lock_guard<std::mutex> lock(mutex);
                    if (instances.find(Addr) == instances.end()) {
                        instances[Addr] = ResponseEndpoint.address().to_string();
                        std::cout << "Instance Found At: " << ResponseEndpoint.address() << " : "
                                  << ResponseEndpoint.port() << " "
                                  << socket.local_endpoint().port() << "\n";
                    }
                }

                break;

            case PayloadType::DiscoveryResponse:

                if (std::memcmp(packet.Payload,
                                OmniDiscoveryResponse,
                                sizeof(OmniDiscoveryResponse)) == 0) {
                    uint32_t ip_addr = ResponseEndpoint.address().to_v4().to_uint();
                    std::lock_guard<std::mutex> lock(mutex);
                    if (instances.find(ip_addr) == instances.end()) {
                        instances[ip_addr] = ResponseEndpoint.address().to_string();
                        std::cout << "Instance Found At: " << ResponseEndpoint.address() << " : "
                                  << ResponseEndpoint.port() << " "
                                  << socket.local_endpoint().port() << "\n";
                    }
                }

                break;

            case PayloadType::ConnectResponse:
                break;

            case PayloadType::ConnectRequest:
                break;

            default:
                break;
            }
        }
    };
};
#endif
