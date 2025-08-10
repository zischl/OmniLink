#ifndef IOLINK_H
#define IOLINK_H

#pragma once

#include <thread>
#include <string>
#include <vector>
#include <atomic>
#include <Windows.h>
#include <WinUser.h>
#include <hidusage.h>

class OmniCap {
public:
	UINT MouseX;
	UINT MouseY;

	OmniCap();

	void ToggleWindowCap(bool state = false);
	void ToggleInputEventCap(HWND hwnd, bool state = false);
	
	UINT RawInputSize;

	void (OmniCap::*InputProc)(LPARAM& lParam) = nullptr;
	void ToggleInputCap(HWND hwnd, bool state = false);
	
	void InputProcInit(LPARAM& lParam);
	void InputProcCallback(LPARAM& lParam);
	void OmniCap::VoidExitCallback(LPARAM& lParam);


private:

	std::atomic_bool MouseEventCapStatus;
	HWINEVENTHOOK WinCapHook = NULL;
	HHOOK MouseCapHook = NULL;


	static void CALLBACK WinMvEventProc(
		HWINEVENTHOOK hWinEventHook,
		DWORD event,
		HWND hwnd,
		LONG idObject,
		LONG idChild,
		DWORD idEventThread,
		DWORD dwmsEventTime
	);


};

class OmniSynth {
public:

};

#endif