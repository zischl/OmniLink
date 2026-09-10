#pragma once

#include "OmniEnums.h"
#include "OmniRouterContext.h"
#include <Windows.h>
#include <atomic>
#include <functional>
#include <thread>

class WindowDragCap
{
  public:
    explicit WindowDragCap(OmniRouterContext& Context);
    ~WindowDragCap();

    void WindowMoveListener(bool State = false);

    std::function<void(HWND, DeviceMap)> WindowDragCallback = nullptr;

  private:
    OmniRouterContext&    Router;
    HWINEVENTHOOK         WinCapHook = NULL;
    std::atomic<uint64_t> DragSessionId{0};

    static WindowDragCap* DragCapInstance;

    // Called on the moment drag starts to boot up a drag session
    void StartDragTracking(HWND Hwnd);

    // Called on drag release, Invalidates the active session
    void StopDragTracking();

    // Runs and stops from Start/Stop DragTracking functions.
    // Handles first edge crossing and subsequent movement mirroring.
    void DragTrackingLoop(HWND Hwnd, uint64_t SessionId);

    // Final drop gurantees remote window position mirroring sits at the exact correct
    // position for the last time and takes focus to that window.
    void FinalizeDrop(HWND Hwnd, DeviceMap Edge, const RECT& Pos, int GripX, int GripY);

    static void CALLBACK WinMvEventProc(
        HWINEVENTHOOK HWinEventHook,
        DWORD         Event,
        HWND          Hwnd,
        LONG          IDObject,
        LONG          IDChild,
        DWORD         IDEventThread,
        DWORD         DWMSEventTime
    );
};
