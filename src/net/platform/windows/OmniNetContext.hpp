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

    static void GetLocals(uint8_t Family, std::vector<sockaddr_in>* Buffer);

    static sockaddr_in CreateAddress(PCSTR IP, unsigned short Port);

    static SOCKET CreateSocket();

    static bool ConnectSesssion(const sockaddr_in& Address, const SOCKET& SocketR);

    static HANDLE CreateIOCP(DWORD MaxThreads = 1);

    static bool BindIOCP(HANDLE IOCP, SOCKET Socket, ULONG_PTR CompletionKey);

    static bool BindReceiver(PCSTR IP, unsigned int Port, SOCKET& Socket);

    static void PostWSARecv(SOCKET Socket, WSABUF* Buffer, OVERLAPPED* OVStruct);
};

#endif // OMNINETCONTEXT_H
