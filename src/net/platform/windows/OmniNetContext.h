#ifndef OMNINETCONTEXT_H
#define OMNINETCONTEXT_H

#pragma once

#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

class OmniNetContext
{
  private:
    int WSResult;
    int WinsockInit();

  public:
    WSADATA wsaData;

    OmniNetContext();

    static void GetLocals(uint8_t family, std::vector<sockaddr_in>* Buffer);

    static sockaddr_in CreateAddress(PCSTR IP, unsigned short port);

    static SOCKET CreateSocket();

    static bool ConnectSesssion(const sockaddr_in& address, const SOCKET& socketR);

    static HANDLE CreateIOCP(DWORD MaxThreads = 1);

    static bool BindIOCP(HANDLE IOCP, SOCKET Socket, ULONG_PTR CompletionKey);

    static bool BindReceiver(PCSTR IP, unsigned int port, SOCKET& socket);
};

#endif // OMNINETCONTEXT_H
