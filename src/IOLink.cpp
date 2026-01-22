#include <IOLink.h>

OmniCap::OmniCap() {
	POINT pos = {};
	GetCursorPos(&pos);
	MouseX = pos.x;
	MouseY = pos.y;

	RawInputSize = 48;

	MonitorRes MonRes = Device::GetMonitorResolution();
	ResHeight = MonRes.Height;
	ResWidth = MonRes.Width;
}

void OmniCap::WindowMoveListener(bool state) {
	if (WinCapHook == NULL && state == true) {
		WinCapHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, NULL, WinMvEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
	}
	else if (WinCapHook != NULL && state == false) {
		UnhookWinEvent(WinCapHook);
	}
}


void CALLBACK OmniCap::WinMvEventProc(
	HWINEVENTHOOK hWinEventHook,
	DWORD event,
	HWND hwnd,
	LONG idObject,
	LONG idChild,
	DWORD idEventThread,
	DWORD dwmsEventTime
) {
	static std::atomic_bool EventStatus;
	EventStatus.store(true);
	switch (event) {
	case EVENT_SYSTEM_MOVESIZESTART:
	{
		std::thread EventThread([hwnd]() {
			HWND hwnd_ = hwnd;
			RECT pos = {};
			while (EventStatus.load()) {
				GetWindowRect(hwnd_, &pos);
				OutputDebugStringA((std::to_string(pos.right) + "\n").c_str());
				std::this_thread::sleep_for(std::chrono::milliseconds(400));

			}
			});
		EventThread.detach();
	}
	break;
	case EVENT_SYSTEM_MOVESIZEEND:
		EventStatus.store(false);
		break;
	}

}

void OmniCap::ToggleEdgeProbe(HWND hwnd, bool state) {
	MouseEventCapStatus.store(true);
	std::atomic_bool* MouseEventStatus = &MouseEventCapStatus;
	std::thread EventThread([hwnd, MouseEventStatus, this]() {
		HWND hwnd_ = hwnd;
		POINT pos = {};
		while (true) {

			while (MouseEventStatus->load()) {
				GetCursorPos(&pos);
				MouseX = pos.x;
				MouseY = pos.y;

				for (auto& [name, cond] : Conditions) {
					if (cond(MouseX, MouseY)) {
						ToggleInputCapture(hwnd, true);
						MouseEventStatus->store(false);
						const RECT CuLockPos = { MouseX, MouseY, MouseX, MouseY };
						ClipCursor(&CuLockPos);
						OutputDebugStringA("true");
						break;
					}
				}


				std::this_thread::sleep_for(std::chrono::milliseconds(150));
			}

			MouseEventStatus->store(true);


			while (MouseEventStatus->load()) {
				GetCursorPos(&pos);
				

				unsigned uMx = MouseX;
				unsigned uMy = MouseY;

				bool InsideCheck = (uMx <= ResWidth) & (uMy <= ResHeight);

				if (InsideCheck) {
					ToggleInputCapture(hwnd, false);
					ClipCursor(NULL);
					SetCursorPos(uMx, uMy);
					OutputDebugStringA("false");
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(150));
			}

		}
		});
	EventThread.detach();

}



void OmniCap::AddEdgeCondition(DeviceMap Index)
{
	switch (Index) {
	case DeviceMap::L1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return x <= 0 && (y > 0 && y < ResHeight);
			});
		break;

	case DeviceMap::R1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return x >= ResWidth && (y > 0 && y < ResHeight);
			});
		break;

	case DeviceMap::U1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return y <= 0 && (x > 0 && x < ResWidth);
			});
		break;

	case DeviceMap::D1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return y >= ResHeight && (x > 0 && x < ResWidth);
			});
		break;

	case DeviceMap::LU1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return x <= 0 && y <= 0;
			});
		break;

	case DeviceMap::RU1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return x >= ResWidth && y <= 0;
			});
		break;

	case DeviceMap::LD1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return x <= 0 && y >= ResHeight;
			});
		break;

	case DeviceMap::RD1:
		ConditionManager.Add(Index, [&](int x, int y) {
			return x >= ResWidth && y >= ResHeight;
			});
		break;
	}

}


void OmniCap::ToggleInputCapture(HWND hwnd, bool state) {
	RAWINPUTDEVICE InputDevices[2];
	InputDevices[0].hwndTarget = hwnd;
	InputDevices[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	InputDevices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;

	InputDevices[1].hwndTarget = hwnd;
	InputDevices[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	InputDevices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;

	if (state) {
		InputDevices[0].dwFlags = RIDEV_INPUTSINK;
		InputDevices[1].dwFlags = RIDEV_INPUTSINK;
		RegisterRawInputDevices(InputDevices, 2, sizeof(InputDevices[0]));

		InputProc = &OmniCap::InputProcInit;

	}
	else {
		InputProc = &OmniCap::VoidExitCallback;

		InputDevices[0].dwFlags = RIDEV_REMOVE;
		InputDevices[1].dwFlags = RIDEV_REMOVE;
		RegisterRawInputDevices(InputDevices, 2, sizeof(InputDevices));

	}

}


void OmniCap::InputProcInit(LPARAM& lParam) {
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &RawInputSize, sizeof(RAWINPUTHEADER));

	InputProc = &OmniCap::InputProcCallback;

	OnInitialMouseCapture(MouseX, MouseY);
}

void OmniCap::InputProcCallback(LPARAM& lParam) {

	std::vector<BYTE> Buffer(RawInputSize);
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, Buffer.data(), &RawInputSize, sizeof(RAWINPUTHEADER));
	RAWINPUT* input = (RAWINPUT*)Buffer.data();

	if (input->header.dwType == RIM_TYPEMOUSE) {
		MouseX += input->data.mouse.lLastX;
		MouseY += input->data.mouse.lLastY;
		OutputDebugStringA((std::to_string(MouseX) + " " + std::to_string(MouseY) + "\n").c_str());
		OnMouseCapture(*input);
	}
	else if (input->header.dwType == RIM_TYPEKEYBOARD) {
		USHORT key = input->data.keyboard.MakeCode;
		OnKeyboardCapture(*input);
	}


}

void OmniCap::VoidExitCallback(LPARAM& lParam) {
	return;
}


void OmniSynth::SetMouseCursor(int X, int Y)
{
	MouseX = X;
	MouseY = Y;
}


void OmniSynth::ProcMouse(int x, int y)
{
	SetCursorPos(x, y);
}

void OmniSynth::ProcKey(INPUT& input)
{
	SendInput(1, &input, sizeof(input));
}

void OmniSynth::ProcKey(KeyData& input)
{
	INPUT InputStruct;
	InputStruct.ki.wVk = 0;
	InputStruct.ki.wScan = input.MakeCode;
	InputStruct.ki.dwFlags = KEYEVENTF_SCANCODE;

	if (input.Flags & RI_KEY_BREAK) {
		InputStruct.ki.dwFlags |= KEYEVENTF_KEYUP;
	}
	if (input.Flags & RI_KEY_E0) {
		InputStruct.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
	}
	SendInput(1, &InputStruct, sizeof(InputStruct));
}

