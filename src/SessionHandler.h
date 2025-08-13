#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H



#pragma once
#include "WinForge.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#include <string>
#include <thread>
#include <chrono>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "IPHLPAPI.lib")

#define MEMALLOC(size) HeapAlloc(GetProcessHeap(), 0, size)
#define FREE(size) HeapFree(GetProcessHeap(), 0, size)


#define OmniMTU 1450
#define OmniHeaderSize 3

enum BufferType {
	OP_RECV,
	OP_SEND
};

enum PacketType : uint8_t {
	Frame
};

enum ChunkType : uint8_t {
	FrameStart,
	FrameData,
	FrameEnd
};

struct SEND_BUF {
	OVERLAPPED OVStruct;
	WSABUF TransmitBuffer[2];
	BufferType Type;
};


struct RECV_BUF {
	OVERLAPPED OVStruct = {};
	WSABUF TransmitBuffer;
	BufferType Type;
	char* data;
	sockaddr_in addr;
	INT addr_len;
};

struct OmniHeader {
	PacketType PacketType;
	uint8_t Target;
};

struct OmniChunkedHeader : OmniHeader {
	uint8_t ChunkIndex;
};

class sessions {
private:
	int WSResult;
	
	PIP_ADAPTER_ADDRESSES locals = NULL;
	ULONG locals_size = 15000;

	int WinsockInit();
	void GetLocals();


public:
	WSADATA wsaData;
	HANDLE IOCP = NULL;

	sessions();

	sockaddr_in CreateAddress(PCSTR IP, unsigned short port);
	SOCKET CreateSocket();
};



class session {
private:
	sessions Sessions;
	WSADATA wsaData;
	WinForge* Link = nullptr;
	int WSResult;
	
	sockaddr_in address;
	SOCKET socketR;
	HANDLE IOCP = NULL;
	
	int MTU = 0;
	int SPoolHead = 0;
	SEND_BUF TransmitPool[256];

	int RPoolHead = 0;
	RECV_BUF RecvPool[256];
	CHAR RecvBufferPool[256 * (OmniMTU + OmniHeaderSize)];

	CHAR FinalRecvBuffer[256 * (OmniMTU + OmniHeaderSize)];

	OmniChunkedHeader CHeaderPool[256];

	int RecvChunkStart = 0;
	int RecvChunkEnd = 0;
	int RecvChunkLen = 0;

	typedef std::chrono::steady_clock Clock;
	typedef std::chrono::time_point<Clock> TimePoint;
	typedef std::chrono::milliseconds Mlliseconds;
	TimePoint start;

	


	inline void PreSetBufferMTU() {
		int addr_size = sizeof(sockaddr_in);


		for (int BufferCount = 0; BufferCount < 256; BufferCount++) {
			TransmitPool[BufferCount].OVStruct = {};
			TransmitPool[BufferCount].Type = OP_SEND;
			TransmitPool[BufferCount].TransmitBuffer[0].len = MTU;
			TransmitPool[BufferCount].TransmitBuffer[1].len = OmniHeaderSize;


			RecvPool[BufferCount].OVStruct = {};
			RecvPool[BufferCount].data = &RecvBufferPool[BufferCount * (MTU)];
			RecvPool[BufferCount].TransmitBuffer.buf = RecvPool[BufferCount].data;
			RecvPool[BufferCount].TransmitBuffer.len = MTU + OmniHeaderSize;
			RecvPool[BufferCount].Type = OP_RECV;
			RecvPool[BufferCount].addr_len = addr_size;

		}
		
	}

public:
	
	session(sessions& sessions, PCSTR IP, unsigned short port, int MTU_Size, WinForge* Link_);

	
	void RegIOCP(SOCKET& socket);

	void CreateSesssionIOCP(PCSTR IP, unsigned short port);
	void InitReceiver(unsigned int port);
	void ChunkedSend(CHAR* data, int data_size);


	int _requestHandshake();
	int _verifyHandshake();
	int _establishLink();


	inline void PostWSARecv() {
		DWORD flags = 0;

		WSResult = WSARecvFrom(
			socketR,
			&RecvPool[RPoolHead].TransmitBuffer,
			1,
			NULL,
			&flags,
			(sockaddr*)&RecvPool[RPoolHead].addr,
			&RecvPool[RPoolHead].addr_len,
			&RecvPool[RPoolHead].OVStruct,
			NULL
		);
	};

	inline void SessionSend(CHAR* data, int MTU) {
		SEND_BUF* SendBuffer = new SEND_BUF;
		SendBuffer->OVStruct = {};
		SendBuffer->TransmitBuffer[0].buf = data;
		SendBuffer->TransmitBuffer[0].len = MTU;
		SendBuffer->Type = OP_SEND;


		WSASendTo(socketR, SendBuffer->TransmitBuffer, 1, NULL, 0, (sockaddr*)&address, sizeof(address), &SendBuffer->OVStruct, NULL);

	}


};




#endif