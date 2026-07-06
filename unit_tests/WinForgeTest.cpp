#pragma once
#include "WinForge.h"
#include <cassert>
#include <iostream>

LRESULT CALLBACK TestWProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void WinForgeTest()
{
    std::cout << "[RUN] WinForgeTest\n";

    HINSTANCE hInst = GetModuleHandle(NULL);
    WinConfig config(L"TestWindowClass", 800, 600, L"Test Window", NULL);

    HWND hwnd = WindowInit(config, hInst, SW_HIDE, TestWProc);
    assert(hwnd != NULL);

    RECT rect;
    BOOL getRectSuccess = GetWindowRect(hwnd, &rect);
    assert(getRectSuccess);
    assert((rect.right - rect.left) == 800);
    assert((rect.bottom - rect.top) == 600);

    // Clean up
    DestroyWindow(hwnd);
    UnregisterClass(L"TestWindowClass", hInst);

    std::cout << "[PASS] WinForgeTest\n";
}
