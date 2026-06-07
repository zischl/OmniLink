#ifndef UNICODE
#define UNICODE
#endif

#ifndef OMNILINK_H
#define OMNILINK_H

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "resource.h"

#include "OmniCore.h"
#include "OmniGUI.h"

#include <Windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dcomp.h>
#include <directxmath.h>
#include <dxgi1_5.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dcomp.lib")
#include <comdef.h>
#include <wincodec.h>

#include <nvEncodeAPI.h>
#pragma comment(lib, "nvencodeapi.lib")

#define WM_TRAYICON (WM_USER + 1)

class OmniLink : public OmniCore
{
  private:
    HINSTANCE hInstance;
    int nCmdShow;

    HWND hwnd = 0;
    OmniGUI* GUI = nullptr;

    NOTIFYICONDATAW TrayIconData = {};

    DWORD FrameTimeLimitW = 15;
    std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(15 * 1000000);

    std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
        std::chrono::steady_clock::now();

    MSG msg = {};

    void OmniMainLoop();

    void InitTrayIcon(HWND hwnd);

    static LRESULT CALLBACK WProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  public:
    OmniLink(HINSTANCE hInst, int nCmdShow) : hInstance(hInst), nCmdShow(nCmdShow) {};

    void OmniMain(HINSTANCE hInstance, int nCmdShow);
};

#endif
