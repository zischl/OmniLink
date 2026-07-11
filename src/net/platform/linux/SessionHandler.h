#pragma once
#include <cstdint>
#include <string>

template <uint32_t MTU = 1450> class OmniNetSession
{
  public:
    OmniNetSession() {}
    ~OmniNetSession() {}
    void Connect(const std::string& host, int port) {}
    void Disconnect() {}

    template <void (*PacketHandlerFn)(char*, uint32_t, uint8_t, void*)>
    void SessionStart(void* Context)
    {
    }
};
