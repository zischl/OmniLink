#include <asio.hpp>
#include <iostream>
#include <thread>

void create_listener(asio::ip::tcp::socket& socket) {
    uint32_t length;
    std::vector<char> buffer;
    while (true) {
        asio::read(socket, asio::buffer(&length, sizeof(length)));
        buffer.resize(length);
        asio::read(socket, asio::buffer(buffer.data(), length));
        std::cout << "Received: " << std::string(buffer.begin(), buffer.end()) << std::endl;
    }
}

void start_listener(asio::io_context& io, const std::string& ip, unsigned short port) {
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);
    asio::ip::tcp::socket socket(io);
    socket.connect(endpoint);
    std::cout << "listener active!\n";
    create_listener(socket);
}

void send(asio::ip::tcp::socket& socket) {
    while (true) {
        std::string msg;
        std::cout << " : ";
        std::cin >> msg;
        if (msg == "exit") {
            break;
        }
        uint32_t length = msg.size();
        asio::write(socket, asio::buffer(&length, sizeof(length)));
        asio::write(socket, asio::buffer(msg));

    }
}

void start_sender(asio::io_context& io, unsigned short port) {
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
    asio::ip::tcp::socket socket(io);
    acceptor.accept(socket);
    std::cout << "Instance Connected!\n";
    send(socket);
}


int main()
{
    std::cout << "Server : ";
    unsigned short server;
    std::cin >> server;
    std::cout << "Client : ";
    unsigned short client;
    std::cin >> client;
    std::cout << "Server is starting..." << std::endl;

    asio::io_context io_context;

    std::thread sender([&]() {
        start_sender(io_context, server);
        });
    std::this_thread::sleep_for(std::chrono::seconds(10));
    std::thread listener([&]() {
        start_listener(io_context, "127.0.0.1", client);
        });

    sender.join();
    listener.join();

    std::cout << "Server shutting down. Press Enter to exit...";
    std::cin.get();
    return 0;
}