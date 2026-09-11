#include "WindowDragCap.h"
#include "OmniLogger.h"
#include "SessionHandler.h"
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

// Calculates remote window position based on source window position
// and source/target device resolutions
static void ComputeEdgeTarget(
    DeviceMap   Edge,
    const RECT& WindowPosition,
    uint32_t    LocalResW,
    uint32_t    LocalResH,
    uint32_t    TargetResW,
    uint32_t    TargetResH,
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
        OutTargetX = WindowPosition.left - static_cast<int>(LocalResW);
        break;
    case DeviceMap::L1:
    case DeviceMap::LU1:
    case DeviceMap::LD1:
        OutTargetX = static_cast<int>(TargetResW) + WindowPosition.left;
        break;
    default:
        break;
    }

    switch (Edge) {
    case DeviceMap::D1:
    case DeviceMap::RD1:
    case DeviceMap::LD1:
        OutTargetY = WindowPosition.top - static_cast<int>(LocalResH);
        break;
    case DeviceMap::U1:
    case DeviceMap::RU1:
    case DeviceMap::LU1:
        OutTargetY = static_cast<int>(TargetResH) + WindowPosition.top;
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
    DeviceMap PrevEdge       = DeviceMap::C0;
    int       InitialGripX   = 0;
    int       InitialGripY   = 0;
    uint16_t  WindowID       = 0;

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

        OmniNetSession<OmniMTU>* NetSession = nullptr;
        const DeviceMap          Candidate  = Grid[Row][Column];
        if (Candidate != DeviceMap::C0 && (NetSession = Router.GetWindowSession(Candidate))) {
            ActiveEdge = Candidate;
        } else if (Row != 1 && Column != 1) {
            if ((NetSession = Router.GetWindowSession(Grid[1][Column]))) {
                ActiveEdge = Grid[1][Column];
            } else if ((NetSession = Router.GetWindowSession(Grid[Row][1]))) {
                ActiveEdge = Grid[Row][1];
            }
        }

        if (NetSession) {
            // If transitioning directly from one remote edge to another,
            // gotta cancel the old one first
            if (EdgeCrossState && PrevEdge != ActiveEdge) {
                auto* OldSession = Router.GetWindowSession(PrevEdge);
                if (OldSession) {
                    OmniNet::OmniHeader CancelHeader;
                    CancelHeader.Target     = 0;
                    CancelHeader.PacketType = OmniNet::PacketType::ProcWinDrag;
                    CancelHeader.Flags      = 0;

                    OmniWinDragPacket CancelPacket = {};
                    CancelPacket.Action            = WinDragAction::Cancel;
                    CancelPacket.Edge              = PrevEdge;
                    CancelPacket.WindowID          = WindowID;
                    OldSession->SessionSend(
                        reinterpret_cast<CHAR*>(&CancelPacket),
                        sizeof(OmniWinDragPacket),
                        CancelHeader
                    );
                }
                if (WindowDragCallback) {
                    WindowDragCallback(Hwnd, PrevEdge, WinDragAction::Cancel);
                }
                WindowID       = 0;
                EdgeCrossState = false;
            }

            int      TargetX    = 0;
            int      TargetY    = 0;
            uint32_t TargetResW = 0;
            uint32_t TargetResH = 0;

            Router.GetDeviceResolution(ActiveEdge, TargetResW, TargetResH);
            ComputeEdgeTarget(
                ActiveEdge, WindowPos, ResW, ResH, TargetResW, TargetResH, TargetX, TargetY
            );

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
                PrevEdge       = ActiveEdge;
                Packet.Action  = WinDragAction::Begin;

                if (WindowDragCallback) {
                    WindowID = WindowDragCallback(Hwnd, ActiveEdge, WinDragAction::Begin);
                }
                Packet.WindowID = WindowID;

                NetSession->SessionSend(
                    reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
                );
            } else {
                Packet.Action   = WinDragAction::Move;
                Packet.WindowID = WindowID;
                NetSession->SessionSend(
                    reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
                );
            }
        } else if (EdgeCrossState) {
            EdgeCrossState = false;
            auto* Session  = Router.GetWindowSession(PrevEdge);
            if (Session) {
                OmniNet::OmniHeader Header;
                Header.Target     = 0;
                Header.PacketType = OmniNet::PacketType::ProcWinDrag;
                Header.Flags      = 0;

                OmniWinDragPacket Packet = {};
                Packet.Action            = WinDragAction::Cancel;
                Packet.Edge              = PrevEdge;
                Packet.WindowID          = WindowID;
                Session->SessionSend(
                    reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
                );
            }
            if (WindowDragCallback) {
                WindowDragCallback(Hwnd, PrevEdge, WinDragAction::Cancel);
            }
            WindowID = 0;
            PrevEdge = DeviceMap::C0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (EdgeCrossState && DragCapInstance == this) {
        FinalizeDrop(Hwnd, PrevEdge, WindowPos, InitialGripX, InitialGripY, WindowID);
    }
}

void WindowDragCap::FinalizeDrop(
    HWND Hwnd, DeviceMap Edge, const RECT& Pos, int GripX, int GripY, uint16_t SubStreamID
)
{
    auto*          NetSession = Router.GetWindowSession(Edge);
    const uint32_t ResW       = Router.ResWidth.load(std::memory_order_relaxed);
    const uint32_t ResH       = Router.ResHeight.load(std::memory_order_relaxed);

    RECT FinalPos = Pos;
    GetWindowRect(Hwnd, &FinalPos);
    int WinW = FinalPos.right - FinalPos.left;
    int WinH = FinalPos.bottom - FinalPos.top;

    int      TargetX    = 0;
    int      TargetY    = 0;
    uint32_t TargetResW = 0;
    uint32_t TargetResH = 0;

    Router.GetDeviceResolution(Edge, TargetResW, TargetResH);
    ComputeEdgeTarget(Edge, FinalPos, ResW, ResH, TargetResW, TargetResH, TargetX, TargetY);

    if (NetSession) {
        OmniNet::OmniHeader Header;
        Header.Target     = 0;
        Header.PacketType = OmniNet::PacketType::ProcWinDrag;
        Header.Flags      = 0;

        OmniWinDragPacket Packet = {};
        Packet.Action            = WinDragAction::Drop;
        Packet.Edge              = Edge;
        Packet.WindowX           = static_cast<int16_t>(TargetX);
        Packet.WindowY           = static_cast<int16_t>(TargetY);
        Packet.WindowWidth       = static_cast<uint16_t>(WinW);
        Packet.WindowHeight      = static_cast<uint16_t>(WinH);
        Packet.CursorGripX       = static_cast<int16_t>(GripX);
        Packet.CursorGripY       = static_cast<int16_t>(GripY);
        Packet.WindowID          = SubStreamID;

        NetSession->SessionSend(
            reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
        );

        if (WindowDragCallback) {
            WindowDragCallback(Hwnd, Edge, WinDragAction::Drop);
        }
    }
}
