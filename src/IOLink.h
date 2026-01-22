#ifndef IOLINK_H
#define IOLINK_H

#pragma once

#include "OmniTypes.h"
#include "Helper.h"

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
	USHORT MakeCode;
	USHORT Flags;
};

class OmniCap {
public:
	OmniCap();
	
	// Mouse cursor position used by both edge detection and high performance input capture
	int MouseX = 0;
	int MouseY = 0;

	
	/// ########################################################################################## ///
	///	Display Edge Detection For the Mouse							    					   ///
	/// ########################################################################################## ///
	
	unsigned int ResWidth = 0;
	unsigned int ResHeight = 0;

	FlowMorph<int, int, DeviceMap> ConditionManager;



	void ToggleEdgeProbe(HWND hwnd, bool state = false);

	void AddEdgeCondition(DeviceMap Index);

	/// ########################################################################################## ///
	/// High Perofrmance Input Capture															   ///
	/// ########################################################################################## ///

	void (OmniCap::* InputProc)(LPARAM& lParam) = nullptr;
	void ToggleInputCapture(HWND hwnd, bool state = false);

	// Initial mouse input event proc used for calculating the size of the raw input struct
	void InputProcInit(LPARAM& lParam);

	// Default mouse input event proc for high performance input capturing
	void InputProcCallback(LPARAM& lParam);

	// Termination sequence for input capturing process
	void VoidExitCallback(LPARAM& lParam);

	void (*OnMouseCapture)(RAWINPUT& input) = nullptr;

	void (*OnKeyboardCapture)(RAWINPUT& RawInput) = nullptr;

	void (*OnInitialMouseCapture)(int MouseX, int MouseY) = nullptr;

	//void (*InputCaptureEvent)(RAWINPUT& RawInput) = nullptr;

	/// ########################################################################################## ///
	/// Window Move Event Detection																   ///
	/// ########################################################################################## ///

	void WindowMoveListener(bool state = false);


private:

	std::unordered_map<DeviceMap, std::function<bool(int, int)>>& Conditions = ConditionManager.conditions;

	std::mutex ConditionMutex;

	std::atomic_bool MouseEventCapStatus;
	HWINEVENTHOOK WinCapHook = NULL;
	HHOOK MouseCapHook = NULL;
	UINT RawInputSize;


	//Callback for window movement detection
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
	int MouseX = 0;
	int MouseY = 0;

	//Sets current cursor position using absolute pixel cordinates
	void static ProcMouse(int x, int y);

	//Simulate keyboard button actions
	void static ProcKey(INPUT& input);

	//Simulate keyboard button actions 
	void static ProcKey(KeyData& input);

	void SetMouseCursor(int MouseX, int MouseY);

	//Move cursor by pixel count rather than set cursor to an exact position
	//Set current cursor position before using this function in order to avoid incorrect starting points
	void inline MvMouse(int toX, int toY)
	{
		MouseX += toX;
		MouseY += toY;

		SetCursorPos(MouseX, MouseY);
	}

	//Returns true if the current registered mouse position matches with the give positions
	bool inline CheckMousePos(int MX, int MY)
	{
		if (MX != MouseX && MY != MouseY) 
			{ return false; }
		else return true;
	}

	MouseXY inline GetCursorPos() {

	}
};

#endif