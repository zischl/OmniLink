#ifndef UNICODE
#define UNICODE
#endif

#ifndef OMNITYPES_H
#define OMNITYPES_H

#pragma once

#include <iostream>
#include <variant>
#include <WinSock2.h>

enum CoreCommands {
	ScanInstances
};

struct ArraySwapLayout {
	int index1 = 0;
	int index2 = 1;

};

using FuncArgTypes = std::variant<ArraySwapLayout>;

//template <typename ArgType>
//struct Command {
//	CoreCommands CommandType = 0;
//	ArgType Args;
//
//};


/// <summary>
/// Custom Array Based Bounded Ring Buffer, Note that this queue will overwrite if used beyond the limit.
/// Used as a deque alternative with a max push limit per drain.
/// </summary>
template <typename Type, unsigned int size>
struct BurstQ {
	std::array<Type, size> Queue;
	unsigned int Head = 0;
	unsigned int Tail = 0;
	unsigned int _mask = 2;

	BurstQ() {
		_mask = size;
	}

	inline void push(const Type& item) {
		Queue[Head] = item;
		Head = (Head + 1) % _mask;

	}

	inline void pop() {
		Tail = (Tail + 1) % _mask;
	}
};


class session;

struct OmniInstance {
	char InstanceName[16];
	uint32_t InstanceIP = NULL;
	char IPv4_String[16] = {};
	session* InstanceSession = nullptr;
};


namespace OmniNet 
{
	enum BufferType {
		OP_RECV,
		OP_SEND
	};

	enum PacketType : uint8_t {
		FrameStart,
		FrameData,
		FrameEnd,
		Command
	};

	struct OmniHeader {
		PacketType PacketType;
		uint8_t Target;
		uint8_t Reserved;
	};

	struct SEND_BUF {
		OVERLAPPED OVStruct;
		WSABUF TransmitBuffer[2];
		BufferType Type;
	};


	struct RECV_BUF {
		OVERLAPPED OVStruct = {};
		WSABUF TransmitBuffer = {};
		BufferType Type = OP_RECV;
		char* data = nullptr;
		sockaddr_in addr = {0};
		int addr_len = 0;
	};

	template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize>
	struct IOContextChunkPool
	{
		uint32_t PoolHead = 0;
		ContextType ContextPool[PoolSize];
		char BufferPool[PoolSize * (ChunkSize + 1)] = "";		// extra chunk for safety
		uint32_t CurrentChunkUsage = 0;

		uint32_t _mask = PoolSize - 1;							// reserved 


		inline void PushChunk() {
			PoolHead = (PoolHead + 1) & _mask;
		}

		inline void PushFinalChunk(uint32_t FinalChunkSize) {
			CurrentChunkUsage = PoolHead * ChunkSize;
			CurrentChunkUsage += FinalChunkSize;
		}

		inline bool TryPushFinalChunk(uint32_t FinalChunkSize) {
			CurrentChunkUsage = PoolHead * ChunkSize;
			CurrentChunkUsage += FinalChunkSize;
			if (CurrentChunkUsage > PoolSize * ChunkSize) {
				std::cout << "OmniNet Chunk Pool Overflowing : Packet May Be Corrupted !\n";
				PoolHead = 0;
				CurrentChunkUsage = 0;
				return false;
			}
			return true;
		}
		
		inline void ResetChunk() {
			PoolHead = 0;
			CurrentChunkUsage = 0;
		}
	};

}

#endif 