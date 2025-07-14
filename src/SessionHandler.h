#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H



#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")


class sessions {
private:
	WSADATA wsaData;
	int _requestHandshake();
	int _verifyHandshake();
	int _establishLink();
	void RegIOCP(SOCKET& socket);

	std::string _GetLocalAddr();

	enum BufferType {
		OP_RECV,
		OP_SEND
	};

	struct BufferContext {
		sockaddr_in address;
		int addrLen;
		WSABUF Buffer;
		char BufferData[1400];
		BufferType Type;
		OVERLAPPED* OVStruct;
	};

	struct TransmitStruct {
		WSABUF TransmitBuffer;
		OVERLAPPED OVStruct;
	 };


public:
	sockaddr_in address;
	SOCKET socketR;
	HANDLE IOCP = NULL;
	int _init_winsock();
	sockaddr_in _create_address(PCSTR IP, unsigned short port);
	SOCKET _create_socket();
	void CreateConnection(PCSTR IP, unsigned short port);
	void ChunkedSend(CHAR* data, int data_size, int MTU);
	int  openSession();
	
};


#endif