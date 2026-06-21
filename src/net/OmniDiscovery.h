#ifndef OMNIDISCOVERY_H
#define OMNIDISCOVERY_H

#pragma once
#include "OmniEnums.h"
#include "system_probe_impl.h"

#include <asio.hpp>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// U might wonder what or who Liss is or why even, but i will be taking that to my grave it seems.
constexpr uint32_t OmniLiss = 0x4F4D4E49;
constexpr char OmniDiscoveryRequest[32] = "OmniLink Liss Where";
constexpr char OmniDiscoveryResponse[32] = "OmniLink Liss Who";

/* #pragma pack(push, 1)
struct LinkingPayload
{
    uint8_t OmniKey[24];
    uint8_t Nonce[8];

    void Serialize(char* out[32])
    {
        std::memcpy(out, OmniKey, sizeof(OmniKey));
        std::memcpy(out + sizeof(OmniKey), Nonce, sizeof(Nonce));
    }
};
#pragma pack(pop) */

enum class PayloadType : uint8_t {
    DiscoveryRequest = 0x01,
    DiscoveryResponse = 0x02,
    IdentifyRequest = 0x03,
    IdentifyResponse = 0x04,
    LinkRequest = 0x05,
    LinkResponse = 0x06,
};

struct ProbeEvent
{
    PayloadType Mode;
    NetLinkState LinkState;
    uint32_t InstanceIP;
};

#pragma pack(push, 1)
struct OmniPayloadBase
{
    PayloadType Type;
    uint16_t PayloadLen = 0;
    char Payload[32] = {};
};

struct OmniDiscoveryPacket : public OmniPayloadBase
{
    uint32_t Liss = OmniLiss;

    static const OmniDiscoveryPacket& From(const OmniPayloadBase& base)
    {
        return *reinterpret_cast<const OmniDiscoveryPacket*>(&base);
    }

    static const OmniDiscoveryPacket* From(const OmniPayloadBase* base)
    {
        return reinterpret_cast<const OmniDiscoveryPacket*>(base);
    }
};
#pragma pack(pop)

class OmniDiscovery
{
  protected:
    // IP Addresses and Instance Names Map
    std::unordered_map<uint32_t, std::string> instances;
    std::mutex mutex;
    std::atomic_bool state{true};

    uint32_t InstanceIP = 0;

  public:
    OmniDiscovery(const std::string& InstanceName, uint32_t _LocalIP, uint16_t port);

    void PopulateInstances(int Runtime);

    // Returns an unordered_map of uint32_t IP Addresses and their names
    std::unordered_map<uint32_t, std::string> get();

    /// <summary>
    /// Send out broadcast requests. Note that AwaitInstances should be running on
    /// the other device for Scan to work.
    /// </summary>
    void Scan(int runtime);
    std::atomic_bool ScanState{false};

    // Sends a direct connection request payload to a specific discovered target IP.
    void SendCustomPayload(const std::string& TargetIP,
                           uint16_t TargetPort,
                           const OmniPayloadBase& Packet);

    void
    SendCustomPayload(uint32_t TargetIPv4, uint16_t TargetPort, const OmniPayloadBase& Payload);

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
            OmniDiscoveryPacket packet;
            asio::ip::udp::endpoint ResponseEndpoint;

            while (state.load()) {

                size_t MsgLen =
                    socket.receive_from(asio::buffer(&packet, sizeof(packet)), ResponseEndpoint);

                if (MsgLen >= sizeof(OmniDiscoveryPacket) && packet.Liss == 0x4F4D4E49) {
                    switch (packet.Type) {
                    case PayloadType::DiscoveryRequest: {
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

                            bool event = false;

                            {
                                std::lock_guard<std::mutex> lock(mutex);
                                if (instances.find(Addr) == instances.end()) {
                                    instances[Addr] = ResponseEndpoint.address().to_string();
                                    std::cout << "Instance Found At: " << ResponseEndpoint.address()
                                              << " : " << ResponseEndpoint.port() << " "
                                              << socket.local_endpoint().port() << "\n";
                                    event = true;
                                }
                            }

                            if (event) {
                                Callback(ProbeEvent{
                                    PayloadType::DiscoveryRequest, NetLinkState::INACTIVE, Addr});
                            }
                        }

                        break;
                    }

                    case PayloadType::DiscoveryResponse: {
                        if (std::memcmp(packet.Payload,
                                        OmniDiscoveryResponse,
                                        sizeof(OmniDiscoveryResponse)) == 0) {
                            uint32_t Addr = ResponseEndpoint.address().to_v4().to_uint();
                            bool event = false;

                            {
                                std::lock_guard<std::mutex> lock(mutex);
                                if (instances.find(Addr) == instances.end()) {
                                    instances[Addr] = ResponseEndpoint.address().to_string();
                                    std::cout << "Instance Found At: " << ResponseEndpoint.address()
                                              << " : " << ResponseEndpoint.port() << " "
                                              << socket.local_endpoint().port() << "\n";

                                    OmniDiscoveryPacket ResponsePacket{
                                        PayloadType::IdentifyRequest,
                                    };
                                    Device::RetrieveUserName(ResponsePacket.Payload);
                                    ResponsePacket.PayloadLen = Device::MAX_CNLEN;
                                    ResponsePacket.Liss = OmniLiss;

                                    socket.send_to(
                                        asio::buffer(&ResponsePacket, sizeof(ResponsePacket)),
                                        ResponseEndpoint);

                                    event = true;
                                }
                            }

                            if (event) {
                                Callback(ProbeEvent{
                                    PayloadType::DiscoveryResponse, NetLinkState::INACTIVE, Addr});
                            }
                        }

                        break;
                    }

                    case PayloadType::IdentifyRequest: {
                        OmniDiscoveryPacket ResponsePacket{
                            PayloadType::IdentifyResponse,
                        };

                        ResponsePacket.PayloadLen = Device::MAX_UNLEN;
                        ResponsePacket.Liss = OmniLiss;

                        std::strncpy(ResponsePacket.Payload,
                                     instances[InstanceIP].c_str(),
                                     sizeof(packet.Payload) - 1);

                        packet.Payload[sizeof(packet.Payload) - 1] = '\0';

                        socket.send_to(asio::buffer(&ResponsePacket, sizeof(ResponsePacket)),
                                       ResponseEndpoint);

                        break;
                    }

                    case PayloadType::IdentifyResponse: {
                        uint32_t Addr = ResponseEndpoint.address().to_v4().to_uint();

                        bool event = false;
                        {
                            std::lock_guard<std::mutex> lock(mutex);
                            if (!(instances.find(Addr) == instances.end())) {
                                instances[Addr] = packet.Payload;

                                std::cout << "Instance " << instances[Addr]
                                          << " Identified At: " << ResponseEndpoint.address()
                                          << " : " << ResponseEndpoint.port() << " "
                                          << socket.local_endpoint().port() << "\n";
                                event = true;
                            }
                        }

                        if (event) {
                            Callback(ProbeEvent{
                                PayloadType::IdentifyResponse, NetLinkState::INACTIVE, Addr});
                        }

                        break;
                    }

                    case PayloadType::LinkRequest: {
                        uint32_t Addr = ResponseEndpoint.address().to_v4().to_uint();
                        Callback(ProbeEvent{PayloadType::LinkRequest, NetLinkState::LINKING, Addr});
                        break;
                    }
                    case PayloadType::LinkResponse: {
                        uint32_t Addr = ResponseEndpoint.address().to_v4().to_uint();
                        NetLinkState LinkState = static_cast<NetLinkState>(packet.Payload[0]);

                        Callback(ProbeEvent{PayloadType::LinkRequest, LinkState, Addr});
                    }
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

    asio::ip::udp::socket socket;

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

            case PayloadType::LinkRequest:
                break;

            default:
                break;
            }
        }
    };
};
#endif
