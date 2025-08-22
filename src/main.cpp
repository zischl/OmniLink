#ifndef UNICODE
#define UNICODE
#endif

#pragma once
#include <OmniLink.h>

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{
	OmniLink OmniLink;
	//OmniLink.OmniMain(hInstance, nCmdShow);
	OmniLink.test3(hInstance, nCmdShow);
	//Nv.NVCleanup();
}



