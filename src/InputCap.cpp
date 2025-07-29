#include <InputCap.h>

void InputCap::ToggleWindowCap(bool state) {
	if (WinCapHook == NULL && state == true) {
		WinCapHook = SetWinEventHook(EVENT_SYSTEM_MOVESIZESTART, EVENT_SYSTEM_MOVESIZEEND, NULL, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
	}
	else if (WinCapHook != NULL && state == false) {
		UnhookWinEvent(WinCapHook);
	}
}


void CALLBACK InputCap::WinEventProc(
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