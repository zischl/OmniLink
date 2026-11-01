#pragma once

#include "OmniEnums.hpp"
#include "WindowOperationTypes.hpp"
#include <Windows.h>
#include <atomic>
#include <functional>
#include <thread>

// Encodes local window position into a normalized ratio entry position values.
// Outputs RatioX and RatioY. Takes in WindowPosition and local resolution.
void EncodeEdgePosition(
    DeviceMap   Edge,
    const RECT& WindowPosition,
    uint32_t    ResWidth,
    uint32_t    ResHeight,
    int16_t&    RatioX,
    int16_t&    RatioY
);

// Decodes normalized ratio back into screen coordinates for windoww positioning.
// Outputs TargetX and TargetY. Takes in RatioX and RatioY and local resolution.
void DecodeEdgePosition(
    DeviceMap Edge,
    int16_t   RatioX,
    int16_t   RatioY,
    uint32_t  ResWidth,
    uint32_t  ResHeight,
    int&      TargetX,
    int&      TargetY
);

// Window drag detection module. Monitors system-wide drag events via WinEvent hook
// and reports raw window/cursor position each poll tick.
class OmniDragLink
{
  public:
    OmniDragLink() = default;
    ~OmniDragLink();

    // Simply toggles window move hook install/uninstall
    void WindowMoveListener(bool State = false);

    // Exclusion check, returns true to skip tracking drags for this window
    std::function<bool(HWND)> WindowExclusionCheck = nullptr;

    // Called every poll tick during an active drag with the window rect and cursor pos
    std::function<void(HWND, const RECT&, POINT)> OnDragMove = nullptr;

    // Called on EVENT_SYSTEM_MOVESIZEEND when a drag completes
    std::function<void(HWND)> OnDragEnd = nullptr;

  private:
    HWINEVENTHOOK         WinCapHook = NULL;
    std::atomic<uint64_t> DragSessionId{0};

    static OmniDragLink* Instance;

    // Increments session ID and launches DragTrackingLoop on a detached thread
    void StartDragTracking(HWND Hwnd);

    // Invalidates the active session and the tracking thread exits on its next iteration
    void StopDragTracking();

    // Polls window rect and cursor pos each tick and calls OnDragMove until session ends
    void DragTrackingLoop(HWND Hwnd, uint64_t SessionId);

    // Main callback called by the windows event hook registered through WindowMoveListener
    // On move start if accepted from the WindowExclusionCheck, calls StartDragTracking
    // On move end calls StopDragTracking
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
