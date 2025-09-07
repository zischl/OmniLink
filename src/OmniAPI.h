#ifndef OMNIAPI_H
#define OMNIAPI_H

#pragma once
#include "OmniTypes.h"


class OmniLink;

class OmniAPI
{
public:
	static void Ignite(OmniLink& OmniLinkInstance);

	static void SwapDeviceLayout();

	static void Scan();

private:
	static inline OmniLink* App = nullptr;
	//static inline HANDLE* Event = nullptr;
};

#endif