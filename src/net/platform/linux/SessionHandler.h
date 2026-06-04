#pragma once
#include <string>

class session {
public:
    session();
    ~session();
    void Connect(const std::string& host, int port);
    void Disconnect();
};
