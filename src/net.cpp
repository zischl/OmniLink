#include <asio.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <unordered_set>



//class connections {
//
//};
//
//void create_listener(asio::ip::tcp::socket& socket) {
//    uint32_t length;
//    std::vector<char> buffer;
//    while (true) {
//        asio::read(socket, asio::buffer(&length, sizeof(length)));
//        buffer.resize(length);
//        asio::read(socket, asio::buffer(buffer.data(), length));
//        std::cout << "Received: " << std::string(buffer.begin(), buffer.end()) << std::endl;
//    }
//}
//
//void start_listener(asio::io_context& io, const std::string& ip, unsigned short port) {
//    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);
//    asio::ip::tcp::socket socket(io);
//    socket.connect(endpoint);
//    std::cout << "listener active!\n";
//    create_listener(socket);
//}
//
//void send(asio::ip::tcp::socket& socket) {
//    while (true) {
//        std::string msg;
//        std::cout << " : ";
//        std::cin >> msg;
//        if (msg == "exit") {
//            break;
//        }
//        uint32_t length = msg.size();
//        asio::write(socket, asio::buffer(&length, sizeof(length)));
//        asio::write(socket, asio::buffer(msg));
//
//    }
//}
//
//void start_sender(asio::io_context& io, unsigned short port) {
//    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
//    asio::ip::tcp::socket socket(io);
//    acceptor.accept(socket);
//    std::cout << "Instance Connected!\n";
//    send(socket);
//}


class Instances {
protected:
    std::unordered_set<std::string> instances;
    std::mutex mutex;
    std::atomic_bool state{ true };

public:
    Instances(unsigned short port) {
        discovery_port = port;
    }

    void populateInstances(int Runtime) {
        std::thread responder([&]() {
            start_responder(io_context, discovery_port);
            });

        std::thread broadcaster([&]() {
            while (state) {
                start_broadcast(io_context, discovery_port);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            });

        std::this_thread::sleep_for(std::chrono::seconds(Runtime));
        state = false;

        if (broadcaster.joinable()) broadcaster.join();
        if (responder.joinable()) responder.detach();
    }

    std::unordered_set<std::string> get() {
        std::lock_guard<std::mutex> lock(mutex);
        return instances;
    }

private:
    unsigned short discovery_port;

    asio::io_context io_context;


    void start_broadcast(asio::io_context& io, unsigned short discovery_port) {
        asio::ip::udp::socket socket(io);
        socket.open(asio::ip::udp::v4());
        socket.set_option(asio::socket_base::reuse_address(true));
        socket.set_option(asio::socket_base::broadcast(true));

        asio::ip::udp::endpoint broadcast_endpoint(asio::ip::make_address("192.168.1.255"), discovery_port);
        std::string broadcast_msg = "OmniLink REQUEST RESPONSE";
        socket.send_to(asio::buffer(broadcast_msg), broadcast_endpoint);

        /*std::this_thread::sleep_for(std::chrono::seconds(1));

        std::array <char, 32> response_buffer;
        asio::ip::udp::endpoint response_endpoint;

        std::this_thread::sleep_for(std::chrono::seconds(2));
        size_t msg_len = socket.receive_from(asio::buffer(response_buffer), response_endpoint);

        msg_len = socket.receive_from(asio::buffer(response_buffer), response_endpoint);
        std::string response(response_buffer.data(), msg_len);
        std::cout << "Response Found At: " << response_endpoint.address().to_string() << " : " << response << "\n";*/
    }

    void start_responder(asio::io_context& io, unsigned short discovery_port) {
        asio::ip::udp::socket socket(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), discovery_port));

        std::array <char, 32> response_buffer;
        asio::ip::udp::endpoint response_endpoint;

        while (state) {
            size_t msg_len = socket.receive_from(asio::buffer(response_buffer), response_endpoint);
            std::string message(response_buffer.data(), msg_len);

            if (message == "OmniLink REQUEST RESPONSE") {
                std::string response = "OmniLink RESPONSE";
                response_endpoint.port(discovery_port);
                socket.send_to(asio::buffer(response), response_endpoint);
                if (instances.find(response_endpoint.address().to_string()) == instances.end()) {
                    std::cout << "Instance Found At: " << response_endpoint.address() << " : " << response_endpoint.port() << " " << socket.local_endpoint().port() << "\n";
                    std::lock_guard<std::mutex> lock(mutex);
                    instances.insert(response_endpoint.address().to_string());

                }
            }
            else if (message == "OmniLink RESPONSE") {
                std::cout << "Response Found At: " << response_endpoint.address().to_string() << " : " << message << "\n";
            }
        }
    }
};
