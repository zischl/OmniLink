#ifndef OMNITYPES_H
#define OMNITYPES_H

#pragma once

#include "SessionHandler.h"


enum CoreCommands {
	SwapInstanceLayout
};

struct OmniInstance {
	std::string InstanceName;
	uint32_t InstanceIP = NULL;
	char IPv4_String[16] = {};
	session* InstanceSession = nullptr;
};

#endif 