#ifndef IOLINK_H
#define IOLINK_H

#pragma once

#include "system_probe_impl.h"
#include <unordered_map>
#include <mutex>
#include <functional>
#include <thread>
#include <string>
#include <vector>
#include <atomic>
#include <Windows.h>
#include <WinUser.h>
#include <hidusage.h>


struct MouseXY {
	int X = 0;
	int Y = 0;

	MouseXY(int x, int y)
	{
		X = x;
		Y = y;
	}
};

struct KeyData
{
	unsigned char Key;
};

class OmniCap {
public:
	unsigned int MouseX = 0;
	unsigned int MouseY = 0;

	unsigned int ResWidth = 0;
	unsigned int ResHeight = 0;

	OmniCap();

	void ToggleWindowCap(bool state = false);
	void ToggleInputEventCap(HWND hwnd, bool state = false);

	UINT RawInputSize;

	void (OmniCap::* InputProc)(LPARAM& lParam) = nullptr;
	void ToggleInputCap(HWND hwnd, bool state = false);

	void InputProcInit(LPARAM& lParam);
	void InputProcCallback(LPARAM& lParam);
	void VoidExitCallback(LPARAM& lParam);


private:
	std::unordered_map<std::string, std::function<bool(int, int)>> Conditions;
	std::mutex ConditionMutex;

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
	void static ProcMouse(int x, int y);

	void static ProcKey(INPUT& input);

	void static ProcKey(RAWINPUT& input);

};

#endif