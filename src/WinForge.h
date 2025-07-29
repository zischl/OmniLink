#ifndef UNICODE
#define UNICODE
#endif 

#ifndef WINFORGE_H
#define WINFORGE_H


#pragma once

#include <OmniRenderer.h>
#include "WinCap.h"

#include <string>
#include <future>
#include <mutex>
#include <thread>

#include <Windows.h>
#include <comdef.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct WinConfig {
	wchar_t class_name = L'Something';
	const wchar_t Window_Name;
	UINT wdWidth = 1280;
	UINT wdHeight = 720;
	
	WinConfig(wchar_t ClassName, UINT Width, UINT Height, wchar_t WindowName) : 
		class_name(ClassName),
		wdWidth(Width),
		wdHeight(Height),
		Window_Name(WindowName) {}
};


class WinForge {
public:
	HWND CreateWindowAsync(wchar_t class_name, WNDPROC WProc, HINSTANCE& hInstance, int nCmdShow, D3DDevice D3DDevStruct = {});

	HWND WindowInit(WinConfig& Config, WNDPROC WProc, HINSTANCE& hInstance, int nCmdShow);

	void ContextSwitch() {

	}

private:
	D3D11_DEVICE_CONTEXT_TYPE ContextMode = D3D11_DEVICE_CONTEXT_IMMEDIATE;

	__forceinline void null() {}

};

#endif