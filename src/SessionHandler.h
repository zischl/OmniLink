#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H



#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "IPHLPAPI.lib")

#define MEMALLOC(size) HeapAlloc(GetProcessHeap(), 0, size)
#define FREE(size) HeapFree(GetProcessHeap(), 0, size)


class sessions {
private:
	int WSResult;
	WSADATA wsaData;
	PIP_ADAPTER_ADDRESSES locals;
	ULONG locals_size = 15000;

	int _requestHandshake();
	int _verifyHandshake();
	int _establishLink();
	void RegIOCP(SOCKET& socket);

	std::string _GetLocalAddr();

	enum BufferType {
		OP_RECV,
		OP_SEND
	};


	struct TransmitStruct {
		OVERLAPPED OVStruct;
		WSABUF TransmitBuffer;
		BufferType Type;
	 };


public:
	sockaddr_in address;
	SOCKET socketR;
	HANDLE IOCP = NULL;
	int _init_winsock();
	sockaddr_in _create_address(PCSTR IP, unsigned short port);
	SOCKET _create_socket();

	void GetLocals();
	void CreateSesssionIOCP(PCSTR IP, unsigned short port);
	void InitReceiver(unsigned int port);
	void ChunkedSend(CHAR* data, int data_size, int MTU);
	
};


#endif