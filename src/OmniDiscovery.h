#ifndef OMNIDISCOVERY_H
#define OMNIDISCOVERY_H

#pragma once
#include <asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_map>


class Instances {
protected:
    std::unordered_map<uint32_t, std::string> instances;
    std::mutex mutex;
    std::atomic_bool state{ true };

public:
    Instances(uint16_t port);

    void PopulateInstances(int Runtime);

    std::unordered_map<uint32_t, std::string>* get();

    void Scan(int runtime);
    std::atomic_bool ScanState{ false };

    inline void HandleResponse(asio::ip::udp::socket& socket, std::array <char, 32>& response_buffer, asio::ip::udp::endpoint& response_endpoint) {
        size_t msg_len = socket.receive_from(asio::buffer(response_buffer), response_endpoint);


        if (*response_buffer.data() == *"OmniLink REQUEST RESPONSE") {
            response_endpoint.port(discovery_port);
            socket.send_to(asio::buffer("OmniLink RESPONSE"), response_endpoint);

            if (instances.find(response_endpoint.address().to_v4().to_uint()) == instances.end()) {
                uint32_t addr = response_endpoint.address().to_v4().to_uint();
                std::cout << "Instance Found At: " << response_endpoint.address() << " : " << response_endpoint.port() << " " << socket.local_endpoint().port() << "\n";
                std::lock_guard<std::mutex> lock(mutex);
                instances[response_endpoint.address().to_v4().to_uint()] = response_endpoint.address().to_string();
            }

        }
        else if (*response_buffer.data() == *"OmniLink RESPONSE" && instances.find(response_endpoint.address().to_v4().to_uint()) == instances.end()) {
            std::cout << "Instance Found At: " << response_endpoint.address() << " : " << response_endpoint.port() << " " << socket.local_endpoint().port() << "\n";
            std::lock_guard<std::mutex> lock(mutex);
            instances[response_endpoint.address().to_v4().to_uint()] = response_endpoint.address().to_string();

        }
    }

    /// <summary>
    /// Await for new instance requests or responses to current device's requests on a blocking wait.
    /// Use EndAwait to stop the running.
    /// </summary>
    void AwaitInstances();

    /// <summary>
    /// Same as AwaitInstances but this one runs for a set duration as runtime (seconds).
    /// </summary>
    /// <param name="runtime"></param>
    void AwaitInstances(int runtime);

    /// <summary>
    /// Same as AwaitInstances but this one can be used to add callbacks on instance found event.
    /// Usage ex : Instances->AwaitInstances([]() { dosomthing(); });
    /// </summary>
    /// <typeparam name="Type"></typeparam>
    /// <param name="Callback"></param>
    template <typename Type>
    void AwaitInstances(Type&& Callback) {
        std::thread responder([this, Callback] () {

            asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), discovery_port));

            std::array <char, 32> response_buffer;
            asio::ip::udp::endpoint response_endpoint;

            while (state.load()) {

                size_t msg_len = socket.receive_from(asio::buffer(response_buffer), response_endpoint);


                if (*response_buffer.data() == *"OmniLink REQUEST RESPONSE") {
                    response_endpoint.port(discovery_port);
                    socket.send_to(asio::buffer("OmniLink RESPONSE"), response_endpoint);

                    if (instances.find(response_endpoint.address().to_v4().to_uint()) == instances.end()) {
                        uint32_t addr = response_endpoint.address().to_v4().to_uint();
                        std::cout << "Instance Found At: " << response_endpoint.address() << " : " << response_endpoint.port() << " " << socket.local_endpoint().port() << "\n";
                        std::lock_guard<std::mutex> lock(mutex);
                        instances[response_endpoint.address().to_v4().to_uint()] = response_endpoint.address().to_string();
                        Callback();
                    }

                }
                else if (*response_buffer.data() == *"OmniLink RESPONSE" && instances.find(response_endpoint.address().to_v4().to_uint()) == instances.end()) {
                    std::cout << "Instance Found At: " << response_endpoint.address() << " : " << response_endpoint.port() << " " << socket.local_endpoint().port() << "\n";
                    std::lock_guard<std::mutex> lock(mutex);
                    instances[response_endpoint.address().to_v4().to_uint()] = response_endpoint.address().to_string();
                    Callback();

                }

            }



            });

        responder.detach();
    }


    inline void EndAwait() {
        state.store(false);
    }



private:
    unsigned short discovery_port;

    asio::io_context io_context;

    

};



#endif