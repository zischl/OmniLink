#include "WindowDragCap.hpp"
#include "OmniLogger.hpp"
#include <algorithm>

OmniDragLink* OmniDragLink::Instance = nullptr;

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

void EncodeEdgePosition(
    DeviceMap   Edge,
    const RECT& WindowPosition,
    uint32_t    ResWidth,
    uint32_t    ResHeight,
    int16_t&    RatioX,
    int16_t&    RatioY
)
{
    if (ResWidth == 0 || ResHeight == 0) {
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top);
        return;
    }

    switch (Edge) {
    case DeviceMap::R1: {
        RatioX          = static_cast<int16_t>(WindowPosition.left - static_cast<int>(ResWidth));
        int64_t ScaledY = (static_cast<int64_t>(WindowPosition.top) * 65535LL) / ResHeight;
        RatioY          = static_cast<int16_t>(std::clamp<int64_t>(ScaledY, 0, 65535));
        break;
    }
    case DeviceMap::L1: {
        RatioX          = static_cast<int16_t>(WindowPosition.left);
        int64_t ScaledY = (static_cast<int64_t>(WindowPosition.top) * 65535LL) / ResHeight;
        RatioY          = static_cast<int16_t>(std::clamp<int64_t>(ScaledY, 0, 65535));
        break;
    }
    case DeviceMap::D1: {
        int64_t ScaledX = (static_cast<int64_t>(WindowPosition.left) * 65535LL) / ResWidth;
        RatioX          = static_cast<int16_t>(std::clamp<int64_t>(ScaledX, 0, 65535));
        RatioY          = static_cast<int16_t>(WindowPosition.top - static_cast<int>(ResHeight));
        break;
    }
    case DeviceMap::U1: {
        int64_t ScaledX = (static_cast<int64_t>(WindowPosition.left) * 65535LL) / ResWidth;
        RatioX          = static_cast<int16_t>(std::clamp<int64_t>(ScaledX, 0, 65535));
        RatioY          = static_cast<int16_t>(WindowPosition.top);
        break;
    }
    case DeviceMap::RU1: {
        RatioX = static_cast<int16_t>(WindowPosition.left - static_cast<int>(ResWidth));
        RatioY = static_cast<int16_t>(WindowPosition.top);
        break;
    }
    case DeviceMap::LU1: {
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top);
        break;
    }
    case DeviceMap::RD1: {
        RatioX = static_cast<int16_t>(WindowPosition.left - static_cast<int>(ResWidth));
        RatioY = static_cast<int16_t>(WindowPosition.top - static_cast<int>(ResHeight));
        break;
    }
    case DeviceMap::LD1: {
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top - static_cast<int>(ResHeight));
        break;
    }
    default:
        RatioX = static_cast<int16_t>(WindowPosition.left);
        RatioY = static_cast<int16_t>(WindowPosition.top);
        break;
    }
}

void DecodeEdgePosition(
    DeviceMap Edge,
    int16_t   RatioX,
    int16_t   RatioY,
    uint32_t  ResWidth,
    uint32_t  ResHeight,
    int&      TargetX,
    int&      TargetY
)
{
    if (ResWidth == 0 || ResHeight == 0) {
        TargetX = RatioX;
        TargetY = RatioY;
        return;
    }

    switch (Edge) {
    case DeviceMap::R1: {
        TargetX          = RatioX;
        uint16_t URatioY = static_cast<uint16_t>(RatioY);
        TargetY = static_cast<int>((static_cast<uint64_t>(URatioY) * ResHeight) / 65535ULL);
        break;
    }
    case DeviceMap::L1: {
        TargetX          = static_cast<int>(ResWidth) + RatioX;
        uint16_t URatioY = static_cast<uint16_t>(RatioY);
        TargetY = static_cast<int>((static_cast<uint64_t>(URatioY) * ResHeight) / 65535ULL);
        break;
    }
    case DeviceMap::D1: {
        uint16_t URatioX = static_cast<uint16_t>(RatioX);
        TargetX          = static_cast<int>((static_cast<uint64_t>(URatioX) * ResWidth) / 65535ULL);
        TargetY          = RatioY;
        break;
    }
    case DeviceMap::U1: {
        uint16_t URatioX = static_cast<uint16_t>(RatioX);
        TargetX          = static_cast<int>((static_cast<uint64_t>(URatioX) * ResWidth) / 65535ULL);
        TargetY          = static_cast<int>(ResHeight) + RatioY;
        break;
    }
    case DeviceMap::RU1: {
        TargetX = RatioX;
        TargetY = static_cast<int>(ResHeight) + RatioY;
        break;
    }
    case DeviceMap::LU1: {
        TargetX = static_cast<int>(ResWidth) + RatioX;
        TargetY = static_cast<int>(ResHeight) + RatioY;
        break;
    }
    case DeviceMap::RD1: {
        TargetX = RatioX;
        TargetY = RatioY;
        break;
    }
    case DeviceMap::LD1: {
        TargetX = static_cast<int>(ResWidth) + RatioX;
        TargetY = RatioY;
        break;
    }
    default:
        TargetX = RatioX;
        TargetY = RatioY;
        break;
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

    if (IDObject != OBJID_WINDOW || Hwnd == NULL || !Instance)
        return;

    DWORD WindowPID = 0;
    GetWindowThreadProcessId(Hwnd, &WindowPID);
    if (WindowPID == GetCurrentProcessId())
        return;

    if (!IsWindow(Hwnd) || !IsWindowVisible(Hwnd) || IsIconic(Hwnd))
        return;

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
