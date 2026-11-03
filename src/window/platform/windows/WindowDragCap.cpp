#include "WindowDragCap.hpp"
#include "OmniLogger.hpp"

OmniDragLink* OmniDragLink::Instance = nullptr;

OmniDragLink::OmniDragLink()
{
    Instance = this;
}

OmniDragLink::~OmniDragLink()
{
    WindowMoveListener(false);
    DragSessionId.fetch_add(1, std::memory_order_release);
    if (Instance == this)
        Instance = nullptr;
}

void OmniDragLink::WindowMoveListener(bool State)
{
    if (WinCapHook == NULL && State == true) {
        WinCapHook = SetWinEventHook(
            EVENT_SYSTEM_MOVESIZESTART,
            EVENT_SYSTEM_MOVESIZEEND,
            NULL,
            WinMvEventProc,
            0,
            0,
            WINEVENT_OUTOFCONTEXT
        );
        if (WinCapHook == NULL) {
            Logger::log(
                "OmniDragLink: Failed to install SetWinEventHook! GetLastError={:d}", GetLastError()
            );
        }
    } else if (WinCapHook != NULL && State == false) {
        UnhookWinEvent(WinCapHook);
        WinCapHook = NULL;
    }
}

void CALLBACK OmniDragLink::WinMvEventProc(
    HWINEVENTHOOK HWinEventHook,
    DWORD         Event,
    HWND          Hwnd,
    LONG          IDObject,
    LONG          IDChild,
    DWORD         IDEventThread,
    DWORD         DWMSEventTime
)
{
    (void)HWinEventHook;
    (void)IDChild;
    (void)IDEventThread;
    (void)DWMSEventTime;

    if (IDObject != OBJID_WINDOW) {
        Logger::log("WinMvEventProc Object ID is not OBJID_WINDOW.");
        return;
    }
    if (Hwnd == NULL) {
        Logger::log("WinMvEventProc Window handle (HWND) is NULL.");
        return;
    }
    if (!Instance) {
        Logger::log("WinMvEventProc Instance pointer is null.");
        return;
    }

    DWORD WindowPID = 0;
    GetWindowThreadProcessId(Hwnd, &WindowPID);
    if (WindowPID == GetCurrentProcessId()) {
        Logger::log("WinMvEventProc Window belongs to the current process (PID matched).");
        return;
    }

    if (Event == EVENT_SYSTEM_MOVESIZESTART) {
        if (Instance->WindowExclusionCheck && Instance->WindowExclusionCheck(Hwnd))
            return;

        Instance->StartDragTracking(Hwnd);
    } else if (Event == EVENT_SYSTEM_MOVESIZEEND) {
        Instance->StopDragTracking();
        if (Instance->OnDragEnd) {
            Instance->OnDragEnd(Hwnd);
        }
    }
}

void OmniDragLink::StartDragTracking(HWND Hwnd)
{
    uint64_t CurrentSession = DragSessionId.fetch_add(1, std::memory_order_relaxed) + 1;
    std::thread([this, Hwnd, CurrentSession]() {
        DragTrackingLoop(Hwnd, CurrentSession);
    }).detach();
}

void OmniDragLink::StopDragTracking()
{
    DragSessionId.fetch_add(1, std::memory_order_release);
}

void OmniDragLink::DragTrackingLoop(HWND Hwnd, uint64_t SessionId)
{
    while (SessionId == DragSessionId.load(std::memory_order_relaxed)) {
        if (!IsWindow(Hwnd) || Instance != this)
            return;

        RECT  WindowPos = {};
        POINT CursorPt  = {};
        GetWindowRect(Hwnd, &WindowPos);
        GetCursorPos(&CursorPt);

        if (OnDragMove) {
            OnDragMove(Hwnd, WindowPos, CursorPt);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
