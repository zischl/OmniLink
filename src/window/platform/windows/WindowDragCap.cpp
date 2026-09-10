#include "WindowDragCap.h"
#include "OmniLogger.h"
#include "SessionHandler.h"
#include "WindowDragTypes.h"
#include <algorithm>

WindowDragCap* WindowDragCap::DragCapInstance = nullptr;

WindowDragCap::WindowDragCap(OmniRouterContext& Context) : Router(Context)
{
    DragCapInstance = this;
}

WindowDragCap::~WindowDragCap()
{
    WindowMoveListener(false);
    DragSessionId.fetch_add(1, std::memory_order_release);
    if (DragCapInstance == this)
        DragCapInstance = nullptr;
}

void WindowDragCap::WindowMoveListener(bool State)
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
    } else if (WinCapHook != NULL && State == false) {
        UnhookWinEvent(WinCapHook);
        WinCapHook = NULL;
    }
}

// Calculates remote window position based from source window postion and source device resolution
static void ComputeEdgeTarget(
    DeviceMap   Edge,
    const RECT& WindowPosition,
    uint32_t    ScreenResW,
    uint32_t    ScreenResH,
    int&        OutTargetX,
    int&        OutTargetY
)
{
    OutTargetX = WindowPosition.left;
    OutTargetY = WindowPosition.top;

    switch (Edge) {
    case DeviceMap::R1:
    case DeviceMap::RU1:
    case DeviceMap::RD1:
        OutTargetX = WindowPosition.left - static_cast<int>(ScreenResW);
        break;
    case DeviceMap::L1:
    case DeviceMap::LU1:
    case DeviceMap::LD1:
        OutTargetX = static_cast<int>(ScreenResW) + WindowPosition.left;
        break;
    default:
        break;
    }

    switch (Edge) {
    case DeviceMap::D1:
    case DeviceMap::RD1:
    case DeviceMap::LD1:
        OutTargetY = WindowPosition.top - static_cast<int>(ScreenResH);
        break;
    case DeviceMap::U1:
    case DeviceMap::RU1:
    case DeviceMap::LU1:
        OutTargetY = static_cast<int>(ScreenResH) + WindowPosition.top;
        break;
    default:
        break;
    }
}

void CALLBACK WindowDragCap::WinMvEventProc(
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

    if (IDObject != OBJID_WINDOW || Hwnd == NULL || !DragCapInstance)
        return;

    DWORD WindowPID = 0;
    GetWindowThreadProcessId(Hwnd, &WindowPID);
    if (WindowPID == GetCurrentProcessId())
        return;

    if (!IsWindow(Hwnd) || !IsWindowVisible(Hwnd) || IsIconic(Hwnd))
        return;

    if (Event == EVENT_SYSTEM_MOVESIZESTART) {
        DragCapInstance->StartDragTracking(Hwnd);
    } else if (Event == EVENT_SYSTEM_MOVESIZEEND) {
        DragCapInstance->StopDragTracking();
    }
}

void WindowDragCap::StartDragTracking(HWND Hwnd)
{
    uint64_t CurrentSession = DragSessionId.fetch_add(1, std::memory_order_relaxed) + 1;
    std::thread([this, Hwnd, CurrentSession]() {
        DragTrackingLoop(Hwnd, CurrentSession);
    }).detach();
}

void WindowDragCap::StopDragTracking()
{
    DragSessionId.fetch_add(1, std::memory_order_release);
}

void WindowDragCap::DragTrackingLoop(HWND Hwnd, uint64_t SessionId)
{
    RECT      WindowPos      = {};
    POINT     CursorPt       = {};
    bool      EdgeCrossState = false;
    DeviceMap ResetEdge      = DeviceMap::C0;
    int       InitialGripX   = 0;
    int       InitialGripY   = 0;

    while (SessionId == DragSessionId.load(std::memory_order_relaxed)) {
        if (!IsWindow(Hwnd) || DragCapInstance != this)
            return;

        GetWindowRect(Hwnd, &WindowPos);
        GetCursorPos(&CursorPt);

        int WindowWidth  = WindowPos.right - WindowPos.left;
        int WindowHeight = WindowPos.bottom - WindowPos.top;

        if (!EdgeCrossState) {
            InitialGripX = CursorPt.x - WindowPos.left;
            InitialGripY = CursorPt.y - WindowPos.top;
        }

        const uint32_t ResW = Router.ResWidth.load(std::memory_order_relaxed);
        const uint32_t ResH = Router.ResHeight.load(std::memory_order_relaxed);

        DeviceMap ActiveEdge = DeviceMap::C0;

        static constexpr DeviceMap Grid[3][3] = {
            {DeviceMap::LU1, DeviceMap::U1, DeviceMap::RU1},
            {DeviceMap::L1, DeviceMap::C0, DeviceMap::R1},
            {DeviceMap::LD1, DeviceMap::D1, DeviceMap::RD1}
        };

        const int Column =
            (WindowPos.right > static_cast<int>(ResW)) ? 2 : ((WindowPos.left < 0) ? 0 : 1);
        const int Row =
            (WindowPos.top < 0) ? 0 : ((WindowPos.bottom > static_cast<int>(ResH)) ? 2 : 1);

        const DeviceMap Candidate = Grid[Row][Column];
        if (Candidate != DeviceMap::C0 && Router.GetSessionState(Candidate)) {
            ActiveEdge = Candidate;
        } else if (Row != 1 && Column != 1) {
            if (Router.GetSessionState(Grid[1][Column])) {
                ActiveEdge = Grid[1][Column];
            } else if (Router.GetSessionState(Grid[Row][1])) {
                ActiveEdge = Grid[Row][1];
            }
        }

        auto* NetSession = (ActiveEdge != DeviceMap::C0) ? Router.GetSession(ActiveEdge) : nullptr;

        if (NetSession) {

            int TargetX = 0;
            int TargetY = 0;
            ComputeEdgeTarget(ActiveEdge, WindowPos, ResW, ResH, TargetX, TargetY);

            OmniNet::OmniHeader Header;
            Header.Target     = 0;
            Header.PacketType = OmniNet::PacketType::ProcWinDrag;
            Header.Flags      = 0;

            OmniWinDragPacket Packet = {};
            Packet.Edge              = ActiveEdge;
            Packet.WindowX           = static_cast<int16_t>(TargetX);
            Packet.WindowY           = static_cast<int16_t>(TargetY);
            Packet.WindowWidth       = static_cast<uint16_t>(WindowWidth);
            Packet.WindowHeight      = static_cast<uint16_t>(WindowHeight);
            Packet.CursorGripX       = static_cast<int16_t>(InitialGripX);
            Packet.CursorGripY       = static_cast<int16_t>(InitialGripY);

            if (!EdgeCrossState) {
                EdgeCrossState = true;
                ResetEdge      = ActiveEdge;
                Packet.Action  = WinDragAction::Begin;

                if (WindowDragCallback) {
                    WindowDragCallback(Hwnd, ActiveEdge);
                }

                NetSession->SessionSend(
                    reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
                );
            } else {
                Packet.Action = WinDragAction::Move;
                NetSession->SessionSend(
                    reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
                );
            }
        } else if (EdgeCrossState) {
            EdgeCrossState = false;
            auto* Session  = Router.GetSession(ResetEdge);
            if (Session) {
                OmniNet::OmniHeader Header;
                Header.Target     = 0;
                Header.PacketType = OmniNet::PacketType::ProcWinDrag;
                Header.Flags      = 0;

                OmniWinDragPacket Packet = {};
                Packet.Action            = WinDragAction::Cancel;
                Packet.Edge              = ResetEdge;
                Session->SessionSend(
                    reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
                );
            }
            ResetEdge = DeviceMap::C0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (EdgeCrossState && DragCapInstance == this) {
        FinalizeDrop(Hwnd, ResetEdge, WindowPos, InitialGripX, InitialGripY);
    }
}

void WindowDragCap::FinalizeDrop(HWND Hwnd, DeviceMap Edge, const RECT& Pos, int GripX, int GripY)
{
    auto*          NetSession = Router.GetSession(Edge);
    const uint32_t ResW       = Router.ResWidth.load(std::memory_order_relaxed);
    const uint32_t ResH       = Router.ResHeight.load(std::memory_order_relaxed);

    RECT FinalPos = Pos;
    GetWindowRect(Hwnd, &FinalPos);
    int WinW = FinalPos.right - FinalPos.left;
    int WinH = FinalPos.bottom - FinalPos.top;

    int TargetX = 0;
    int TargetY = 0;
    ComputeEdgeTarget(Edge, FinalPos, ResW, ResH, TargetX, TargetY);

    if (NetSession) {
        OmniNet::OmniHeader Header;
        Header.Target     = 0;
        Header.PacketType = OmniNet::PacketType::ProcWinDrag;
        Header.Flags      = 0;

        OmniWinDragPacket Packet = {};
        Packet.Action       = (Edge != DeviceMap::C0) ? WinDragAction::Drop : WinDragAction::Cancel;
        Packet.Edge         = Edge;
        Packet.WindowX      = static_cast<int16_t>(TargetX);
        Packet.WindowY      = static_cast<int16_t>(TargetY);
        Packet.WindowWidth  = static_cast<uint16_t>(WinW);
        Packet.WindowHeight = static_cast<uint16_t>(WinH);
        Packet.CursorGripX  = static_cast<int16_t>(GripX);
        Packet.CursorGripY  = static_cast<int16_t>(GripY);

        NetSession->SessionSend(
            reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
        );
    }
}
