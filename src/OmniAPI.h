#ifndef OMNIAPI_H
#define OMNIAPI_H

#pragma once
#include "OmniLink.h"



class OmniAPI 
{
public:
	OmniAPI(OmniLink* OmniLinkInstance);

private:
	OmniLink* App = nullptr;

}

#endif