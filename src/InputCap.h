#ifndef INPUTCAP_H
#define INPUTCAP_H

#pragma once

#include <thread>
#include <string>
#include <atomic>
#include <Windows.h>
#include <WinUser.h>

class InputCap {
public:
	void ToggleWindowCap(bool state = false);
	void ToggleMouseCap(bool state = false);

private:
	HWINEVENTHOOK WinCapHook = NULL;


	static void CALLBACK WinEventProc(
		HWINEVENTHOOK hWinEventHook,
		DWORD event,
		HWND hwnd,
		LONG idObject,
		LONG idChild,
		DWORD idEventThread,
		DWORD dwmsEventTime
	);
};

#endif