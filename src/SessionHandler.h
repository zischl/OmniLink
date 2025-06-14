#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H



#pragma once
#include <ws2tcpip.h>
#include <winsock2.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")


class sessions {
private:
	WSADATA wsaData;
	int _requestHandshake();
	int _verifyHandshake();
	int _establishLink();
public:
	int _init_winsock();
	sockaddr_in _create_address(PCSTR IP, unsigned short port);
	SOCKET _create_socket();
	int  openSession();
	
};


#endif