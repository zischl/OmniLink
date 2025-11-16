#ifndef SESSIONHANDLER_H
#define SESSIONHANDLER_H



#pragma once
#include "OmniTypes.h"
#include "OmniLogger.h"
#include "WinForge.h"
#include "IOLink.h"
#include "OmniAPI.h"
#include "Helper.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#include <iostream>
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



class sessions {
private:
	int WSResult;

	int WinsockInit();


public:
	WSADATA wsaData;
	HANDLE IOCP = NULL;

	sessions();

	static void GetLocals(uint8_t family, std::vector<sockaddr_in>* Buffer);

	static sockaddr_in CreateAddress(PCSTR IP, unsigned short port);

	static SOCKET CreateSocket();

	static void ConnectSesssion(const sockaddr_in& address, const SOCKET& socketR);

	static void RegIOCP(HANDLE& IOCP, SOCKET& socket, const ULONG_PTR CompletionKey = 0);

	template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize, typename PacketHandler>
	static void StartCompletionPortHandlerThread(const HANDLE& IOCP, const SOCKET& socket, OmniNet::IOContextChunkPool<ContextType, PoolSize, ChunkSize>& Pool, PacketHandler&& PacketHandlerFn, void* Ctx)
	{
		std::thread StatusQueue([&]()
			{
				while (true) {
					DWORD BufferSize = 0;
					ULONG_PTR EventKey = 0;
					OVERLAPPED* OVStruct = nullptr;

					bool WSResult = GetQueuedCompletionStatus(IOCP, &BufferSize, &EventKey, &OVStruct, INFINITE);
					if (!WSResult)
					{
						OutputDebugStringA((std::to_string(GetLastError()) + "type \n").c_str());
						OutputDebugStringA("Completion Status Get False\n");
						if (OVStruct != nullptr) {
							OutputDebugStringA("Completion Status Get OVStruct Failed\n");
						}
						else {
							OutputDebugStringA("IOCP Thread Going Down...\n");
						}
					}

					OmniNet::RECV_BUF* Buffer = reinterpret_cast<OmniNet::RECV_BUF*>(OVStruct);
					

					switch (Buffer->Type) {
					case OmniNet::OP_RECV:
						// header structure's packet type index = 0 and the header size is 3,
						// going backwards from bytes means minus 2 but since the data stream is an array it would be minus 3 due to indexing. 
						uint8_t BufferHeader = *(Buffer->TransmitBuffer.buf + BufferSize - 3);

						switch (BufferHeader)
						{
						case OmniNet::PacketType::ChunkStart:
							Pool.PushChunk();
							break;
						case OmniNet::PacketType::ChunkData:
							Pool.PushChunk();
							break;
						case OmniNet::PacketType::ChunkEnd:
							if (Pool.TryPushFinalChunk(BufferSize))
							{
								PacketHandlerFn(&Pool.BufferPool[0], Pool.CurrentChunkUsage, BufferHeader, Ctx);
							}
							Pool.ResetChunk();
							break;
						default:
							PacketHandlerFn(Buffer->TransmitBuffer.buf, BufferSize, BufferHeader, Ctx);
						}
							Pool.ResetChunk();
							break;



						PostWSARecv(socket, Pool);
						break;




					}

				}
			}
		);

		StatusQueue.detach();

	};


	static void BindReceiver(PCSTR IP, unsigned int port, SOCKET& socket);


	template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize>
	inline static void PostWSARecv(const SOCKET& socket, OmniNet::IOContextChunkPool<ContextType, PoolSize, ChunkSize>& Pool) {
		DWORD flags = 0;

		int WSResult = WSARecv(
			socket,
			&Pool.ContextPool[Pool.PoolHead].TransmitBuffer,
			1,
			NULL,
			&flags,
			&Pool.ContextPool[Pool.PoolHead].OVStruct,
			NULL
		);

		if (WSAGetLastError() != WSA_IO_PENDING) {
			OutputDebugStringA("Recv Pre Post Failed\n");
		}
	};

};



class session {
private:
	int WSResult;

	sockaddr_in address;
	SOCKET socketR;

	const int MTU;

	uint32_t SPoolHead = 0;
	OmniNet::SEND_BUF TransmitPool[256];
	OmniNet::OmniHeader CHeaderPool[256];

	OmniNet::IOContextChunkPool<OmniNet::RECV_BUF, 256, 1450> RecvPool;



	typedef std::chrono::steady_clock Clock;
	typedef std::chrono::time_point<Clock> TimePoint;
	typedef std::chrono::milliseconds Mlliseconds;
	TimePoint start;




	inline void PreSetBufferMTU() {
		int addr_size = sizeof(sockaddr_in);


		for (int BufferCount = 0; BufferCount < 256; BufferCount++) {
			TransmitPool[BufferCount].OVStruct = {};
			TransmitPool[BufferCount].Type = OmniNet::OP_SEND;
			TransmitPool[BufferCount].TransmitBuffer[0].len = MTU;
			TransmitPool[BufferCount].TransmitBuffer[1].len = OmniHeaderSize;
			TransmitPool[BufferCount].TransmitBuffer[1].buf = reinterpret_cast<CHAR*>(&CHeaderPool[BufferCount]);

			RecvPool.ContextPool[BufferCount].OVStruct = {};
			RecvPool.ContextPool[BufferCount].data = &RecvPool.BufferPool[BufferCount * (MTU)];
			RecvPool.ContextPool[BufferCount].TransmitBuffer.buf = RecvPool.ContextPool[BufferCount].data;
			RecvPool.ContextPool[BufferCount].TransmitBuffer.len = MTU + OmniHeaderSize;
			RecvPool.ContextPool[BufferCount].Type = OmniNet::OP_RECV;
			RecvPool.ContextPool[BufferCount].addr_len = addr_size;

		}

	}

public:

	session(HANDLE& IOCP, PCSTR Local_IP, PCSTR IP, unsigned short port, int MTU_Size, void* Context);

	void (*OnIOCompletion) (CHAR * Buffer, DWORD BufferSize, uint8_t BufferHeader, void* Context) = nullptr;


	inline void SessionSend(CHAR* data, int MTU, const OmniNet::OmniHeader& header) {
		CHeaderPool[SPoolHead].PacketType = header.PacketType;
		CHeaderPool[SPoolHead].Target = header.Target;
		CHeaderPool[SPoolHead].Flags = header.Flags;

		TransmitPool[SPoolHead].TransmitBuffer[1].buf = reinterpret_cast<CHAR*>(&CHeaderPool[SPoolHead]);
		TransmitPool[SPoolHead].TransmitBuffer[0].buf = data;


		WSASend(socketR, TransmitPool[SPoolHead].TransmitBuffer, 2, NULL, 0, &TransmitPool[SPoolHead].OVStruct, NULL);

		SPoolHead = (SPoolHead + 1) & 255;
	}

	inline void ChunkedSend(CHAR* data, int data_size) {
		//start = Clock::now();

		int MTU_slices = data_size - (data_size % MTU);
		CHeaderPool[SPoolHead].PacketType = OmniNet::ChunkStart;
		CHeaderPool[SPoolHead].Target = 0;
		TransmitPool[SPoolHead].TransmitBuffer[0].buf = data;

		WSASend(
			socketR,
			TransmitPool[SPoolHead].TransmitBuffer,
			2,
			NULL, 0,
			&TransmitPool[SPoolHead].OVStruct,
			NULL
		);


		SPoolHead = (SPoolHead + 1) & 255;

		for (int offset = MTU; offset < MTU_slices - MTU; offset += MTU) {
			CHeaderPool[SPoolHead].PacketType = OmniNet::ChunkData;
			CHeaderPool[SPoolHead].Target = 0;
			TransmitPool[SPoolHead].TransmitBuffer[0].buf = data + offset;


			WSASend(
				socketR,
				TransmitPool[SPoolHead].TransmitBuffer,
				2,
				NULL, 0,
				&TransmitPool[SPoolHead].OVStruct,
				NULL
			);


			SPoolHead = (SPoolHead + 1) & 255;

		}



		if (MTU_slices != data_size) {
			CHeaderPool[SPoolHead].PacketType = OmniNet::ChunkData;
		}
		else {
			CHeaderPool[SPoolHead].PacketType = OmniNet::ChunkEnd;
		}
		CHeaderPool[SPoolHead].Target = 0;
		TransmitPool[SPoolHead].TransmitBuffer[0].buf = data + MTU_slices - MTU;



		WSASend(
			socketR,
			TransmitPool[SPoolHead].TransmitBuffer,
			2,
			NULL, 0,
			&TransmitPool[SPoolHead].OVStruct,
			NULL
		);


		SPoolHead = (SPoolHead + 1) & 255;

		if (MTU_slices != data_size) {
			CHeaderPool[SPoolHead].PacketType = OmniNet::ChunkEnd;
			CHeaderPool[SPoolHead].Target = 0;
			TransmitPool[SPoolHead].TransmitBuffer[0].buf = data + MTU_slices;
			TransmitPool[SPoolHead].TransmitBuffer[0].len = data_size % MTU;

			WSASend(
				socketR,
				TransmitPool[SPoolHead].TransmitBuffer,
				2, NULL, 0,
				&TransmitPool[SPoolHead].OVStruct,
				NULL
			);

			TransmitPool[SPoolHead].TransmitBuffer[0].len = MTU;

			SPoolHead = (SPoolHead + 1) & 255;
		}


		//std::cout << std::chrono::duration_cast<std::chrono::nanoseconds> (Clock::now() - start) << "\n";
	}

};




#endif