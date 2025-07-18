
#include "WinForge.h"


HWND CreateWindowAsync(wchar_t class_name, WNDPROC WProc, HINSTANCE& hInstance, int nCmdShow) {

	std::promise<HWND> hwnd_p;
	std::future<HWND> hwnd_f = hwnd_p.get_future();

	std::thread test1([&] {

		const wchar_t CLASS_NAME[] = { class_name };

		unsigned int wdWidth = 1920;
		unsigned int wdHeight = 1080;

		WNDCLASS wc = {};
		wc.lpfnWndProc = WProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = CLASS_NAME;
		RegisterClass(&wc);

		HWND hwnd = CreateWindowEx(
			0,
			CLASS_NAME,
			L"too ez",
			WS_OVERLAPPEDWINDOW,
			0,
			0,
			wdWidth,
			wdHeight,
			NULL,
			NULL,
			hInstance,
			NULL
		);

		hwnd_p.set_value(hwnd);

		if (hwnd == NULL)
		{
			OutputDebugString(L"Window Creation Failed\n");
			return;
		}

		ShowWindow(hwnd, nCmdShow);



		MSG msg = { };
		while (true) {
			while (GetMessage(&msg, NULL, 0, 0) > 0)
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		});


	test1.detach();

	HWND hwnd = hwnd_f.get();

	return hwnd;

}