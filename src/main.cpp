#ifndef UNICODE
#define UNICODE
#endif

#pragma once
#include <OmniLink.h>

#include <iostream>

void* operator new(size_t size)
{
	//std::cout << size << " bytes memory allocated\n";

	return malloc(size);
}

void operator delete(void* memory, size_t size)
{
	free(memory);

	//std::cout << size << " bytes memory freed\n";
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR pCmdLine, _In_ int nCmdShow)
{

	AllocConsole();
	freopen("CONOUT$", "w", stdout);

	OmniLink OmniLink;
	OmniLink.OmniMain(hInstance, nCmdShow);
	//OmniLink.test2(hInstance, nCmdShow);
}



