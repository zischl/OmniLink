#pragma once
#include <string>

template <uint32_t MTU = 1450> class OmniNetSession
{
  public:
    OmniNetSession() {}
    ~OmniNetSession() {}
    void Connect(const std::string& host, int port) {}
    void Disconnect() {}
};
