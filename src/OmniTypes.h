#ifndef OMNITYPES_H
#define OMNITYPES_H

#pragma once

#include <variant>
#include "SessionHandler.h"

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



struct OmniInstance {
	char InstanceName[16];
	uint32_t InstanceIP = NULL;
	char IPv4_String[16] = {};
	session* InstanceSession = nullptr;
};

#endif 