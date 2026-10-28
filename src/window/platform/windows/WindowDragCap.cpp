#include "WindowDragCap.hpp"
#include "OmniLogger.hpp"
#include "SessionHandler.hpp"
#include <algorithm>

OmniDragLink* OmniDragLink::Instance = nullptr;

OmniDragLink::OmniDragLink(OmniRouter& Context) : Router(Context)
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
        TargetX         = RatioX;
        uint16_t RatioY = static_cast<uint16_t>(RatioY);
        TargetY         = static_cast<int>((static_cast<uint64_t>(RatioY) * ResHeight) / 65535ULL);
        break;
    }
    case DeviceMap::L1: {
        TargetX         = static_cast<int>(ResWidth) + RatioX;
        uint16_t RatioY = static_cast<uint16_t>(RatioY);
        TargetY         = static_cast<int>((static_cast<uint64_t>(RatioY) * ResHeight) / 65535ULL);
        break;
    }
    case DeviceMap::D1: {
        uint16_t RatioX = static_cast<uint16_t>(RatioX);
        TargetX         = static_cast<int>((static_cast<uint64_t>(RatioX) * ResWidth) / 65535ULL);
        TargetY         = RatioY;
        break;
    }
    case DeviceMap::U1: {
        uint16_t RatioX = static_cast<uint16_t>(RatioX);
        TargetX         = static_cast<int>((static_cast<uint64_t>(RatioX) * ResWidth) / 65535ULL);
        TargetY         = static_cast<int>(ResHeight) + RatioY;
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

    if (Instance->WindowExclusionCheck && Instance->WindowExclusionCheck(Hwnd))
        return;

    if (Event == EVENT_SYSTEM_MOVESIZESTART) {
        Instance->StartDragTracking(Hwnd);
    } else if (Event == EVENT_SYSTEM_MOVESIZEEND) {
        Instance->StopDragTracking();
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
    RECT           WindowPos    = {};
    POINT          CursorPt     = {};
    EdgeSpatiality SpatialState = EdgeSpatiality::Local;
    DeviceMap      PrevEdge     = DeviceMap::C0;
    int            InitialGripX = 0;
    int            InitialGripY = 0;
    uint16_t       WindowID     = 0;

    while (SessionId == DragSessionId.load(std::memory_order_relaxed)) {
        if (!IsWindow(Hwnd) || Instance != this)
            return;

        GetWindowRect(Hwnd, &WindowPos);
        GetCursorPos(&CursorPt);

        int WindowWidth  = WindowPos.right - WindowPos.left;
        int WindowHeight = WindowPos.bottom - WindowPos.top;

        if (SpatialState == EdgeSpatiality::Local) {
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

        const bool ModifierState = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                                   ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);

        if (SpatialState == EdgeSpatiality::Local && !ModifierState) {
            ActiveEdge = DeviceMap::C0;
            NetSession = nullptr;
        }

        if (NetSession) {
            // If transitioning directly from one remote edge to another,
            // gotta cancel the old one first
            if (SpatialState == EdgeSpatiality::InTransit && PrevEdge != ActiveEdge) {
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
                WindowID     = 0;
                SpatialState = EdgeSpatiality::Local;
            }

            int16_t PacketX = 0;
            int16_t PacketY = 0;
            EncodeEdgePosition(ActiveEdge, WindowPos, ResW, ResH, PacketX, PacketY);

            OmniNet::OmniHeader Header;
            Header.Target     = 0;
            Header.PacketType = OmniNet::PacketType::ProcWinDrag;
            Header.Flags      = 0;

            OmniWinDragPacket Packet = {};
            Packet.Edge              = ActiveEdge;
            Packet.WindowX           = PacketX;
            Packet.WindowY           = PacketY;
            Packet.WindowWidth       = static_cast<uint16_t>(WindowWidth);
            Packet.WindowHeight      = static_cast<uint16_t>(WindowHeight);
            Packet.CursorGripX       = static_cast<int16_t>(InitialGripX);
            Packet.CursorGripY       = static_cast<int16_t>(InitialGripY);

            if (SpatialState == EdgeSpatiality::Local) {
                SpatialState  = EdgeSpatiality::InTransit;
                PrevEdge      = ActiveEdge;
                Packet.Action = WinDragAction::Begin;

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
        } else if (SpatialState == EdgeSpatiality::InTransit) {
            SpatialState  = EdgeSpatiality::Local;
            auto* Session = Router.GetWindowSession(PrevEdge);
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

    if (SpatialState == EdgeSpatiality::InTransit && Instance == this) {
        FinalizeDrop(Hwnd, PrevEdge, WindowPos, InitialGripX, InitialGripY, WindowID);
    }
}

void OmniDragLink::FinalizeDrop(
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

    int16_t RatioX = 0;
    int16_t RatioY = 0;
    EncodeEdgePosition(Edge, FinalPos, ResW, ResH, RatioX, RatioY);

    if (NetSession) {
        OmniNet::OmniHeader Header;
        Header.Target     = 0;
        Header.PacketType = OmniNet::PacketType::ProcWinDrag;
        Header.Flags      = 0;

        OmniWinDragPacket Packet = {};
        Packet.Action            = WinDragAction::Drop;
        Packet.Edge              = Edge;
        Packet.WindowX           = RatioX;
        Packet.WindowY           = RatioY;
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
