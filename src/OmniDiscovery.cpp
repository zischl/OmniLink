#include "OmniDiscovery.h"

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

#ifndef UNICODE
#define UNICODE
#endif

#define UNLEN 256

Instances::Instances(uint16_t port) :
    ComputerName(*WinGetComputerName())
{
    discovery_port = port;
}


void Instances::PopulateInstances(int Runtime)
{
    AwaitInstances(Runtime);
    Scan(Runtime);


}


std::unordered_map<uint32_t, std::string>* Instances::get()
{
    return &instances;
}


char* Instances::WinGetUserName() {
    TCHAR ComputerName[UNLEN + 1];
    DWORD size = UNLEN + 1;

    GetUserName((TCHAR*)ComputerName, &size);

    std::array <char, MAX_COMPUTERNAME_LENGTH + 1> CharArray[MAX_COMPUTERNAME_LENGTH + 1];

#ifndef UNICODE
    WideCharToMultiByte(CP_ACP, 0, ComputerName, -1, CharArray->data(), MAX_COMPUTERNAME_LENGTH + 1, nullptr, nullptr);
#else
    strcpy(CharArray->data(), ComputerName);
#endif

    return CharArray->data();
}

char* Instances::WinGetComputerName() {

    TCHAR ComputerName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(ComputerName) / sizeof(ComputerName[0]);
    GetComputerName(ComputerName, &size);

    std::array <char, MAX_COMPUTERNAME_LENGTH + 1> CharArray[MAX_COMPUTERNAME_LENGTH + 1];
    
#ifndef UNICODE
    WideCharToMultiByte(CP_ACP, 0, ComputerName, -1, CharArray->data(), MAX_COMPUTERNAME_LENGTH + 1, nullptr, nullptr);
#else
    strcpy(CharArray->data(), ComputerName);
#endif

    return CharArray->data();
}


uint32_t Instances::GetLocalIP() {
    return 0;
}


void Instances::Scan(int runtime)
{
    if (!ScanState.load()) {

        ScanState.store(true);

        std::thread broadcaster([this, runtime]() {

            std::chrono::steady_clock::duration Runtime = std::chrono::seconds(runtime);

            std::chrono::time_point<std::chrono::steady_clock> Start = std::chrono::steady_clock::now();

            asio::ip::udp::socket socket(io_context);
            socket.open(asio::ip::udp::v4());
            socket.set_option(asio::socket_base::reuse_address(true));
            socket.set_option(asio::socket_base::broadcast(true));

            asio::ip::udp::endpoint broadcast_endpoint(asio::ip::make_address("192.168.1.255"), discovery_port);

            while ((std::chrono::steady_clock::now() - Start) <= Runtime) {

                socket.send_to(asio::buffer("OmniLink REQUEST RESPONSE"), broadcast_endpoint);

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            ScanState.store(false);

            }
        );

        broadcaster.detach();
    }
    


}



void Instances::AwaitInstances()
{

    std::thread responder([this]() {

        asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), discovery_port));

        std::array <char, 32> response_buffer;
        asio::ip::udp::endpoint response_endpoint;

        while (state.load()) {

            HandleResponse(socket, response_buffer, response_endpoint);

        }

        

        });

    responder.detach();
}


void Instances::AwaitInstances(int runtime)
{

    std::thread responder([this, runtime]() {

        std::chrono::steady_clock::duration Runtime = std::chrono::steady_clock::duration(runtime * 10000000);

        std::chrono::time_point<std::chrono::steady_clock> Start = std::chrono::steady_clock::now();

        asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), discovery_port));

        std::array <char, 32> response_buffer;
        asio::ip::udp::endpoint response_endpoint;

        while ((std::chrono::steady_clock::now() - Start) <= Runtime) {

            HandleResponse(socket, response_buffer, response_endpoint);

        }

        });

    responder.detach();
}