#ifndef UNICODE
#define UNICODE
#endif

#ifndef OMNITYPES_H
#define OMNITYPES_H

#pragma once
#include <functional>
#include <utility>
#include <array>
#include <vector>
#include <thread>
#include <atomic>
#include <optional>
#include <iostream>
#include <variant>
#include <WinSock2.h>

#if defined(_WIN32)
#define OmniDevNameLen MAX_COMPUTERNAME_LENGTH
#elif defined(__linux__)
#define OmniDevNameLen 16
#else
#define OmniDevNameLen 16
#endif


class session;
class WinForge;
//class NvencSession;


enum FeatureFlags {
	fInactive = 0,
	fScreenLink = 1 < 0,
	fWindowLink = 1 < 1,
	fInputLink = 1 < 2,
	fLink = 1 < 3,

};

enum FeatureTypes {
	ScreenLink,
	WindowLink,
	InputLink
};

enum DeviceMap {
	C0,
	L1,
	U1,
	R1,
	D1,
	LU1,
	RU1,
	RD1,
	LD1
};

enum CoreCommands {
	OmniStatus,
	ScanInstances
};

enum CoreCommandsWArgs {
	TESTCOMMAND,
	SwapLayout,
	ConnectDevice
};


struct ArraySwapLayout {
	int index1 = 0;
	int index2 = 1;

};


struct TestArg {
	int x = 0;
};

using FuncArgTypes = std::variant<ArraySwapLayout, DeviceMap, TestArg>;


struct OmniNetCommandType {
	CoreCommandsWArgs CommandType = TESTCOMMAND;
	uint32_t ArgTypeIndex = 0;
	unsigned char* Args = nullptr;
};



struct OmniNetCommand {
	CoreCommandsWArgs CommandType = TESTCOMMAND;
	uint32_t ArgTypeIndex = 0;
	unsigned char* Args = nullptr;
	size_t ArgArrayLength = 0;

	OmniNetCommand(OmniNetCommandType& Command, size_t ArgArrayLen)
	{
		CommandType = Command.CommandType;
		ArgTypeIndex = Command.ArgTypeIndex;
		Args = Command.Args;
		ArgArrayLength = ArgArrayLen;
	}

};	

struct OmniCommand {
	CoreCommandsWArgs CommandType = TESTCOMMAND;
	uint32_t ArgTypeIndex = 0;
	FuncArgTypes Args = TestArg{ 0 };
};


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


struct OmniIP {
	uint32_t InstanceIP = NULL;
	char IPv4_String[16] = {};

};

struct OmniInstance {
	char InstanceName[MAX_COMPUTERNAME_LENGTH + 1] = {};
	uint32_t InstanceIP = NULL;
	char IPv4_String[16] = {};
	uint8_t DevMapIndex = 0;

	OmniInstance() {}

	OmniInstance(uint8_t DevMIndex) {
		DevMapIndex = DevMIndex;
	}

	void Clear()
	{
		memset(&InstanceIP, 0, MAX_COMPUTERNAME_LENGTH + 1);
		InstanceIP = NULL;
		memset(&IPv4_String, 0, 16);
	}

	void Edit(char* InstanceName_, char* IPv4_String_, uint32_t InstanceIP_)
	{
		InstanceIP = InstanceIP_;
		strncpy(IPv4_String, IPv4_String_, 16);
		strncpy(InstanceName, InstanceName_, (OmniDevNameLen + 1));
	}

};

struct OmniActiveInstance : OmniInstance
{
	session* InstanceSession = nullptr;
	std::vector<WinForge*> ActiveWindows;
	uint16_t port = 62485;
	int ActiveFlags = FeatureFlags::fInactive;
	//std::vector<NvencSession> NVEncoderSessions;


	OmniActiveInstance() {}

	OmniActiveInstance(char* InstanceName_, char* IPv4_String_, uint32_t InstanceIP_)
	{
		InstanceIP = InstanceIP_;
		strncpy(IPv4_String, IPv4_String_, 16);
		strncpy(InstanceName, InstanceName_, (OmniDevNameLen + 1));
	}


};

using ActiveInstanceContainer = std::unordered_map<DeviceMap, OmniActiveInstance>;

template <size_t MaxFrameLen>
struct FrameByte
{
	char Frame[MaxFrameLen];
	size_t FrameLen = 0;

	FrameByte(size_t frame_len) { FrameLen = frame_len; }
};


namespace AsyncWorker {

	template <typename ResultPoolType, size_t ResultPoolSize>
	class Cached
	{

	public:
		std::optional<std::thread> Thread;
		std::array<ResultPoolType, ResultPoolSize> ResultPool = {};
		std::atomic_bool status;
		uint32_t head = 1;
		uint32_t tail = 0;


		template <typename func, typename... Args>
		inline void StartSpinThread(func&& Function, Args&&... Arguments)
		{

			status.store(true);

			Thread.emplace(
				[
					&status = this->status, &ResultPool = this->ResultPool,
					&head = this->head,
					&tail = this->tail,
					function = std::forward<func>(Function),
					...args = std::forward<Args>(Arguments)
				]() mutable
				{
					while (status.load())
					{
						ResultPool[head] = std::invoke(function, args...);
						Push(head, tail);
					}
				});
		}



		template <typename func, typename... Args>
		inline void StartWaitThread(size_t timeout, func&& Function, Args&&... Arguments)
		{

			status.store(true);

			Thread.emplace(
				[
					timeout,
					&status = this->status, &ResultPool = this->ResultPool,
					&head = this->head,
					&tail = this->tail,
					function = std::forward<func>(Function),
					...args = std::forward<Args>(Arguments)
				]() mutable
				{
					while (status.load())
					{
						ResultPool[head] = std::invoke(function, args...);
						Push(head, tail);

						Sleep(timeout);
					}
				});
		}


		void EndLoopedThread() {
			status.store(false);
			Thread.reset();
		}


		inline static void Push(uint32_t& head, uint32_t& tail)
		{
			if (head != tail) {
				head = (head + 1) & (ResultPoolSize - 1);
			}
		}

		void Pull()
		{
			if (head != tail) {
				tail = (tail + 1) & (ResultPoolSize - 1);
			}
		}


	};


	class Uncached
	{

	public:
		std::optional<std::thread> Thread;
		std::atomic_bool status;

		template <typename func, typename... Args>
		inline void StartSpinThread(func&& Function, Args&&... Arguments)
		{

			status.store(true);

			Thread.emplace(
				[
					&status = this->status,
					function = std::forward<func>(Function),
					...args = std::forward<Args>(Arguments)
				]() mutable
				{
					while (status.load())
					{
						std::invoke(function, args...);
					}
				});
		}



		template <typename func, typename... Args>
		inline void StartWaitThread(size_t timeout, func&& Function, Args&&... Arguments)
		{

			status.store(true);

			Thread.emplace(
				[
					timeout,
					&status = this->status,
					function = std::forward<func>(Function),
					...args = std::forward<Args>(Arguments)
				]() mutable
				{
					while (status.load())
					{
						std::invoke(function, args...);

						Sleep(timeout);
					}
				});
		}


		void EndLoopedThread() {
			status.store(false);
			Thread.reset();
		}


	};


}


namespace AsyncDispatch
{
	class DispatchBase {
	public:
		std::optional<std::thread> DispatchThread;
		std::atomic_bool DispatcherStatus;
		uint8_t ActiveWorkers = 0;

	};


	template <typename ResultPoolType, size_t ResultPoolSize>
	class Dynamic : public DispatchBase
	{
	public:
		std::vector<AsyncWorker::Cached<ResultPoolType, ResultPoolSize>> Workers;

		Dynamic() {
			Workers.reserve(2);
		}

		template <typename function, typename... args>
		void StartThread(function&& Function, args&&... Args)
		{
			Workers.emplace_back();

		}
	};


	template <typename Worker, typename ResultPoolType, size_t ResultPoolSize, size_t MaxWorkerCount>
	class Fixed : public DispatchBase
	{
	public:
		std::array<AsyncWorker::Cached<ResultPoolType, ResultPoolSize>, MaxWorkerCount> Workers;

		template <typename function, typename... args>
		void StartThreadLoop()
		{
			//StartLoopedThread();
		}

		template <typename function, typename... args>
		void SetDispatchFunc(function&& Function, args&&... Arguments) {
			DispatchThread.emplace([&DispatcherStatus = this->DispatcherStatus, &Func = std::forward<function>(Function), &Args = std::forward<args>(Arguments)]()
				{
					while (DispatcherStatus.load())
					{

					}
				});
		}

	};

};




namespace OmniNet
{
	enum BufferType : uint8_t {
		OP_RECV,
		OP_SEND
	};

	enum PacketType : uint8_t {
		ChunkStart,
		ChunkData,
		ChunkEnd,
		Command,
		ProcMouse,
		ProcKey
	};

	enum FlagTypes : uint8_t {
		VoidArg,
		ArgCount
	};

	struct OmniHeader {
		PacketType PacketType;
		uint8_t Target;
		uint8_t Flags;
	};

	struct SEND_BUF {
		OVERLAPPED OVStruct = {};
		WSABUF TransmitBuffer[2] = { 0 };
		BufferType Type = OP_SEND;
	};


	struct RECV_BUF {
		OVERLAPPED OVStruct = {};
		WSABUF TransmitBuffer = {};
		BufferType Type = OP_RECV;
		char* data = nullptr;
		sockaddr_in addr = { 0 };
		int addr_len = 0;
	};

	template <typename ContextType, uint32_t PoolSize, uint32_t ChunkSize>
	struct IOContextChunkPool
	{
		uint32_t PoolHead = 0;
		ContextType ContextPool[PoolSize];
		char BufferPool[PoolSize * (ChunkSize + 1)] = "";		// extra chunk for safety
		uint32_t CurrentChunkUsage = 0;

		uint32_t _mask = PoolSize - 1;							// Flags 


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


	template <typename ContextType, typename Header, uint32_t PoolSize>
	struct IOContextTransmitRing
	{
		uint32_t PoolHead = 0;
		ContextType ContextPool[PoolSize];
		Header HeaderPool[PoolSize];

		uint32_t _mask = PoolSize - 1;							// Flags 


		inline void PushChunk() {
			PoolHead = (PoolHead + 1) & _mask;
		}

	};
}

#endif 