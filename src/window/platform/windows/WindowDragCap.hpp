#pragma once

#include "OmniEnums.hpp"
#include "OmniRouterContext.hpp"
#include "WindowOperationTypes.hpp"
#include <Windows.h>
#include <atomic>
#include <functional>
#include <thread>

class OmniDragLink
{
  public:
    explicit OmniDragLink(OmniRouter& Context);
    ~OmniDragLink();

    void WindowMoveListener(bool State = false);

    // Returns a uint16_t id which can be used or passed around, as a unique window id helper
    std::function<uint16_t(HWND, DeviceMap, WinDragAction)> WindowDragCallback = nullptr;

  private:
    OmniRouter&           Router;
    HWINEVENTHOOK         WinCapHook = NULL;
    std::atomic<uint64_t> DragSessionId{0};

    static OmniDragLink* Instance;

    // Called on the moment drag starts to boot up a drag session
    void StartDragTracking(HWND Hwnd);

    // Called on drag release, Invalidates the active session
    void StopDragTracking();

    // Runs and stops from Start/Stop DragTracking functions.
    // Handles first edge crossing and subsequent movement mirroring.
    void DragTrackingLoop(HWND Hwnd, uint64_t SessionId);

    // Final drop gurantees remote window position mirroring sits at the exact correct
    // position for the last time and takes focus to that window.
    void FinalizeDrop(
        HWND Hwnd, DeviceMap Edge, const RECT& Pos, int GripX, int GripY, uint16_t WindowID
    );

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
