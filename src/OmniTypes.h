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

template <typename Type>
struct Command {
	CoreCommands CommandType;
	Type Args;

	Command(CoreCommands command, Type args) :
		CommandType(command),
		Args(args) {
	}
};


struct OmniInstance {
	char InstanceName[16];
	uint32_t InstanceIP = NULL;
	char IPv4_String[16] = {};
	session* InstanceSession = nullptr;
};

#endif 