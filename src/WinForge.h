#ifndef UNICODE
#define UNICODE
#endif 

#ifndef WINFORGE_H
#define WINFORGE_H


#pragma once

#include <string>
#include <future>
#include <mutex>
#include <thread>

#include <Windows.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;


HWND CreateWindowAsync(wchar_t class_name, WNDPROC WProc, HINSTANCE& hInstance, int nCmdShow);



#endif