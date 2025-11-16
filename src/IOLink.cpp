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

void OmniCap::ToggleWindowCap(bool state) {
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
				OutputDebugString((std::to_string(pos.right) + "\n").c_str());
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

void OmniCap::ToggleInputEventCap(HWND hwnd, bool state) {
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
				OutputDebugString((std::to_string(MouseX) + "\n").c_str());

				for (auto& [name, cond] : Conditions) {
					if (cond(MouseX, MouseY)) {
						ToggleInputCap(hwnd, true);
						break;
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(150));
			}

			while (MouseEventStatus->load()) {
				GetCursorPos(&pos);
				OutputDebugString((std::to_string(MouseX) + "2nd\n").c_str());

				unsigned uMx = MouseX;
				unsigned uMy = MouseY;

				bool InsideCheck = (uMx <= ResWidth) & (uMy <= ResHeight);

				if (InsideCheck) {
					ToggleInputCap(hwnd, false);
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(150));
			}

		}
		});
	EventThread.detach();

}


void OmniCap::ToggleInputCap(HWND hwnd, bool state) {
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
}

void OmniCap::InputProcCallback(LPARAM& lParam) {

	std::vector<BYTE> Buffer(RawInputSize);
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, Buffer.data(), &RawInputSize, sizeof(RAWINPUTHEADER));
	RAWINPUT* input = (RAWINPUT*)Buffer.data();

	if (input->header.dwType == RIM_TYPEMOUSE) {
		MouseX += input->data.mouse.lLastX;
		MouseY += input->data.mouse.lLastY;
		input->data.mouse.usButtonFlags;
		OutputDebugString((std::to_string(MouseX) + " raw x\n").c_str());
	}
	else if (input->header.dwType == RIM_TYPEKEYBOARD) {
		USHORT key = input->data.keyboard.MakeCode;

		OutputDebugString((std::to_string(key) + " raw key\n").c_str());
	}


}

void OmniCap::VoidExitCallback(LPARAM& lParam) {
	return;
}


void OmniSynth::ProcMouse(int x, int y)
{
	SetCursorPos(x, y);
}

void OmniSynth::ProcKey(INPUT& input)
{
	SendInput(1, &input, sizeof(input));
}

void OmniSynth::ProcKey(RAWINPUT& input)
{
	INPUT InputStruct;
	InputStruct.ki.wVk = 0;
	InputStruct.ki.wScan = input.data.keyboard.MakeCode;
	InputStruct.ki.dwFlags = KEYEVENTF_SCANCODE;

	if (input.data.keyboard.Flags & RI_KEY_BREAK) {
		InputStruct.ki.dwFlags |= KEYEVENTF_KEYUP;
	}
	if (input.data.keyboard.Flags & RI_KEY_E0) {
		InputStruct.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
	}
	SendInput(1, &InputStruct, sizeof(InputStruct));
}
