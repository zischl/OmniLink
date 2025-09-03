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

    HANDLE* CallbackEventOnCompletion = nullptr;

public:
    Instances(uint16_t port, HANDLE* Event);

    void PopulateInstances(int Runtime);

    std::unordered_map<uint32_t, std::string>* get();

    void Scan(int runtime);
    std::atomic_bool ScanState{ false };

    void AwaitInstances();

    void AwaitInstances(int runtime);

    inline void EndAwait() {
        state.store(false);
    }



private:
    unsigned short discovery_port;

    asio::io_context io_context;

    

};



#endif