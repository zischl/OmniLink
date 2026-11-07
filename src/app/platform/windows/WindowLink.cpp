#include "WindowLink.hpp"
#include "OmniConfig.hpp"
#include "OmniInstances.hpp"
#include "OmniLogger.hpp"
#include "OmniRouterContext.hpp"
#include "SpatialRouter.hpp"
#include "StreamPacketDispatch.hpp"
#include "StreamPacketHandlers.cpp"
#include "StreamSessions.hpp"
#include "StreamWindowEventHandlers.cpp"
#include "SubStream.hpp"

// Creates the WinForge render window on Client and attaches the forwarders
OmniNet::PoolConfig SystemWindowLink::SetupClientStreamRenderWindow(
    SystemLinkContext& Ctx,
    SubStreamID        SubStreamID,
    DeviceMap          DeviceID,
    uint32_t           Width,
    uint32_t           Height,
    int16_t            TargetX,
    int16_t            TargetY
)
{
    OmniNetSession<OmniMTU>* NetSession = nullptr;
    if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
        NetSession = Ctx.ActiveInstances->at(DeviceID).InstanceSession.get();
    }

    auto Entry = std::make_unique<WindowStreamEntry>(WindowStreamEntry{
        .Session          = NetSession,
        .SpatialRouter    = &Ctx.SpatialRouter,
        .Router           = &Ctx.Router,
        .ReleaseSubStream = Ctx.ReleaseSubStream,
        .DeviceID         = DeviceID,
        .WindowKey        = SubStreamID,
        .WindowHwnd       = NULL
    });

    WindowStreamEntry* RawEntry = Entry.get();

    OmniWindowEventHandlers EventHandler{};
    EventHandler.Context             = RawEntry;
    EventHandler.OnMouseInput        = StreamWindowOnMouseInput;
    EventHandler.OnKeyInput          = StreamWindowOnKeyInput;
    EventHandler.OnResize            = StreamWindowOnResize;
    EventHandler.OnDecoderReady      = StreamWindowOnDecoderReady;
    EventHandler.OnWindowClose       = StreamWindowOnWindowClose;
    EventHandler.OnWindowDragMove    = StreamWindowOnWindowDragMove;
    EventHandler.OnWindowDragRelease = StreamWindowOnWindowDragRelease;

    OmniNet::PoolConfig PoolConfig = Ctx.StreamSessions.SetupStreamRenderWindow(
        Ctx,
        SubStreamID,
        DeviceID,
        Width,
        Height,
        TargetX,
        TargetY,
        SW_SHOW,
        "Stream Window",
        &EventHandler,
        FeatureTypes::WindowLink
    );

    StreamWindow* Window = Ctx.StreamSessions.FindStreamWindow(SubStreamID);
    if (Window) {
        RawEntry->WindowHwnd = Window->GetHwnd();
    }
    WindowStreamEntries[SubStreamID] = std::move(Entry);
    return PoolConfig;
}

void SystemWindowLink::EnsureWindowFocus(HWND TargetHwnd)
{
    if (!TargetHwnd || !IsWindow(TargetHwnd))
        return;

    HWND CurrentForeground = GetForegroundWindow();
    if (CurrentForeground == TargetHwnd)
        return;

    DWORD TargetThread  = GetWindowThreadProcessId(TargetHwnd, nullptr);
    DWORD CurrentThread = GetCurrentThreadId();

    if (TargetThread != 0 && TargetThread != CurrentThread) {
        AttachThreadInput(CurrentThread, TargetThread, TRUE);
        BringWindowToTop(TargetHwnd);
        SetForegroundWindow(TargetHwnd);
        SetFocus(TargetHwnd);
        AttachThreadInput(CurrentThread, TargetThread, FALSE);
    } else {
        BringWindowToTop(TargetHwnd);
        SetForegroundWindow(TargetHwnd);
        SetFocus(TargetHwnd);
    }
}

void SystemWindowLink::CenterWindowOnDesktop(HWND TargetHwnd)
{
    if (!TargetHwnd || !IsWindow(TargetHwnd)) {
        return;
    }

    RECT Rect{};
    if (GetWindowRect(TargetHwnd, &Rect)) {
        int WinW = Rect.right - Rect.left;
        int WinH = Rect.bottom - Rect.top;

        int ScreenW = GetSystemMetrics(SM_CXSCREEN);
        int ScreenH = GetSystemMetrics(SM_CYSCREEN);
        int CenterX = (std::max)(0, (ScreenW - WinW) / 2);
        int CenterY = (std::max)(0, (ScreenH - WinH) / 2);

        SetWindowPos(
            TargetHwnd, NULL, CenterX, CenterY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
        );
    }
}

// State Machine Transitions
void SystemWindowLink::OnLocalDragStart(HWND Hwnd, const RECT& WindowPos, POINT CursorPos)
{
    DragSession.Phase      = DragPhase::LocalHover;
    DragSession.SourceHwnd = Hwnd;
    DragSession.GripX      = CursorPos.x - WindowPos.left;
    DragSession.GripY      = CursorPos.y - WindowPos.top;
    DragSession.LastPos    = WindowPos;
}

bool SystemWindowLink::OnEdgeCrossBegin(
    SystemLinkContext& Ctx, HWND Hwnd, DeviceMap TargetEdge, const RECT& WindowPos
)
{
    SubStreamID StreamID = HandleWindowDragEvent(Ctx, Hwnd, TargetEdge, WinDragAction::Begin);
    if (StreamID == 0) {
        return false;
    }

    DragSession.Phase      = DragPhase::Crossing;
    DragSession.StreamID   = StreamID;
    DragSession.TargetEdge = TargetEdge;

    auto* NetSession = Ctx.Router.GetWindowSession(TargetEdge);
    if (NetSession) {
        const int WindowWidth  = WindowPos.right - WindowPos.left;
        const int WindowHeight = WindowPos.bottom - WindowPos.top;

        int16_t RatioX = 0;
        int16_t RatioY = 0;
        OmniSpatialRouter::EncodeEdgePosition(
            TargetEdge, WindowPos, DragSession.ResW, DragSession.ResH, RatioX, RatioY
        );

        StreamPackets::TransmitDragPacket(
            NetSession,
            WinDragAction::Begin,
            StreamID,
            TargetEdge,
            RatioX,
            RatioY,
            static_cast<uint16_t>(WindowWidth),
            static_cast<uint16_t>(WindowHeight),
            static_cast<int16_t>(DragSession.GripX),
            static_cast<int16_t>(DragSession.GripY)
        );
    }
    return true;
}

void SystemWindowLink::OnCrossCancel(SystemLinkContext& Ctx, HWND Hwnd)
{
    auto* Session = Ctx.Router.GetWindowSession(DragSession.TargetEdge);
    if (Session) {
        StreamPackets::TransmitDragPacket(
            Session, WinDragAction::Cancel, DragSession.StreamID, DragSession.TargetEdge, 0, 0, 0, 0
        );
    }
    HandleWindowDragEvent(Ctx, Hwnd, DragSession.TargetEdge, WinDragAction::Cancel);
    DragSession = {};
}

void SystemWindowLink::OnRemoteDrop(SystemLinkContext& Ctx, HWND Hwnd)
{
    RECT FinalPos = DragSession.LastPos;
    GetWindowRect(Hwnd, &FinalPos);
    const int WinW = FinalPos.right - FinalPos.left;
    const int WinH = FinalPos.bottom - FinalPos.top;

    int16_t RatioX = 0;
    int16_t RatioY = 0;
    OmniSpatialRouter::EncodeEdgePosition(
        DragSession.TargetEdge, FinalPos, DragSession.ResW, DragSession.ResH, RatioX, RatioY
    );

    auto* NetSession = Ctx.Router.GetWindowSession(DragSession.TargetEdge);
    if (NetSession) {
        StreamPackets::TransmitDragPacket(
            NetSession,
            WinDragAction::Drop,
            DragSession.StreamID,
            DragSession.TargetEdge,
            RatioX,
            RatioY,
            static_cast<uint16_t>(WinW),
            static_cast<uint16_t>(WinH),
            static_cast<int16_t>(DragSession.GripX),
            static_cast<int16_t>(DragSession.GripY)
        );
    }

    HandleWindowDragEvent(Ctx, Hwnd, DragSession.TargetEdge, WinDragAction::Drop);
    DragSession.Phase = DragPhase::Remote;
}

void SystemWindowLink::OnReturnBegin(DeviceMap DeviceID, HWND SourceHwnd, int TargetX, int TargetY)
{
    DragSession.Phase      = DragPhase::Returning;
    DragSession.TargetEdge = DeviceID;
    DragSession.SourceHwnd = SourceHwnd;

    SetWindowPos(
        SourceHwnd, HWND_TOP, TargetX, TargetY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW
    );
}

void SystemWindowLink::OnReturnDrop(DeviceMap DeviceID, HWND SourceHwnd)
{
    (void)DeviceID;
    CenterWindowOnDesktop(SourceHwnd);
    DragSession = {};
}

bool SystemWindowLink::TryInitiateDragFromCaptionTest(
    SystemLinkContext& Ctx, SubStreamID StreamID, HWND TargetHwnd, POINT CursorPoint, RECT WinRect
)
{
    UINT Dpi = GetDpiForWindow(TargetHwnd);
    if (Dpi == 0)
        Dpi = 96;
    int MaxHeaderHeight = MulDiv(64, Dpi, 96);

    if ((CursorPoint.y - WinRect.top) <= MaxHeaderHeight) {
        DWORD_PTR HitResult = HTCLIENT;
        if (SendMessageTimeoutW(
                TargetHwnd,
                WM_NCHITTEST,
                0,
                MAKELPARAM(CursorPoint.x, CursorPoint.y),
                SMTO_ABORTIFHUNG | SMTO_NORMAL,
                25,
                &HitResult
            )) {
            if (HitResult == HTCAPTION) {
                auto IterDevice = SubStream2DeviceRegistry.find(StreamID);
                if (IterDevice != SubStream2DeviceRegistry.end()) {
                    DeviceMap TargetDevice = IterDevice->second;
                    auto*     NetSession   = Ctx.Router.GetWindowSession(TargetDevice);
                    if (NetSession) {
                        StreamPackets::TransmitDragPacket(
                            NetSession, WinDragAction::Begin, StreamID, TargetDevice, 0, 0, 0, 0
                        );
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void SystemWindowLink::HandleReturningWindow(DeviceMap DeviceID)
{
    if (DragSession.Phase == DragPhase::Returning && DragSession.TargetEdge == DeviceID &&
        DragSession.SourceHwnd && IsWindow(DragSession.SourceHwnd)) {
        HWND ReturningHwnd = DragSession.SourceHwnd;
        PostMessage(ReturningHwnd, WM_SYSCOMMAND, 0xF012, 0);
    }
}

OmniStreamer::StreamID SystemWindowLink::StartWindowCaptureStream(
    SystemLinkContext& Ctx, SubStreamID SubStreamID, DeviceMap DeviceID, HWND Hwnd
)
{
    if (!Ctx.ActiveInstances || !Ctx.ActiveInstances->contains(DeviceID)) {
        return 0;
    }

    auto&           Instance = Ctx.ActiveInstances->at(DeviceID);
    SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
    if (!Entry || !Entry->SubStream) {
        return 0;
    }

    StreamConfig Config{};
    Config.WindowHandle = Hwnd;
    RECT rect           = {};
    GetWindowRect(Hwnd, &rect);
    Config.Width    = static_cast<uint32_t>(rect.right - rect.left);
    Config.Height   = static_cast<uint32_t>(rect.bottom - rect.top);
    Config.OnResize = [this, &Ctx, DeviceID, SubStreamID](uint32_t Width, uint32_t Height) {
        auto* NetSession = Ctx.Router.GetWindowSession(DeviceID);
        StreamPackets::TransmitResizePacket(
            NetSession, SubStreamID, static_cast<uint16_t>(Width), static_cast<uint16_t>(Height)
        );
    };
    Config.OnClose = [this, &Ctx, DeviceID, SubStreamID]() {
        Logger::log(
            "Captured window closed for SubStreamID={:d}, DeviceID={:d}",
            SubStreamID,
            static_cast<int>(DeviceID)
        );
        std::thread([this, &Ctx, DeviceID, SubStreamID]() {
            StopWindowCaptureStream(Ctx, SubStreamID);
            if (Ctx.ReleaseSubStream) {
                Ctx.ReleaseSubStream(DeviceID, SubStreamID, true);
            }
        }).detach();
    };

    OmniStreamer::StreamID CaptureStreamID = Ctx.StreamSessions.AddCaptureStream(
        Ctx.GraphicsContext, Entry->SubStream, DeviceID, CaptureMode::WGC_Window, Config
    );
    if (CaptureStreamID == OmniStreamer::InvalidStreamID) {
        Logger::log(
            "Failed to add a capture stream for HWND={:p}"
            "SubStreamID={:d}",
            static_cast<void*>(Hwnd),
            SubStreamID
        );
        return 0;
    }

    Ctx.StreamSessions.RegisterSourceRoute(
        SubStreamID, DeviceID, FeatureTypes::WindowLink, CaptureStreamID, Hwnd
    );
    Hwnd2SubStreamRegistry[Hwnd]          = SubStreamID;
    SubStream2HwndRegistry[SubStreamID]   = Hwnd;
    SubStream2DeviceRegistry[SubStreamID] = DeviceID;

    Logger::log(
        "WindowCapture started for SubStreamID={:d}, HWND={:p}, StreamID={:d}",
        SubStreamID,
        reinterpret_cast<void*>(Hwnd),
        CaptureStreamID
    );
    return CaptureStreamID;
}

void SystemWindowLink::StopWindowCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID)
{
    Ctx.StreamSessions.DestroyCaptureStream(SubStreamID);

    auto IterHwnd = SubStream2HwndRegistry.find(SubStreamID);
    if (IterHwnd != SubStream2HwndRegistry.end()) {
        Hwnd2SubStreamRegistry.erase(IterHwnd->second);
        SubStream2HwndRegistry.erase(IterHwnd);
    }

    SubStream2DeviceRegistry.erase(SubStreamID);

    Logger::log("WindowCapture stopped for SubStreamID={:d}", SubStreamID);
}

SubStreamID SystemWindowLink::HandleWindowDragEvent(
    SystemLinkContext& Ctx, HWND Hwnd, DeviceMap TargetDevice, WinDragAction Action
)
{
    if (!Hwnd) {
        return 0;
    }

    auto       IterSubStreams  = Hwnd2SubStreamRegistry.find(Hwnd);
    const bool SubStreamExists = (IterSubStreams != Hwnd2SubStreamRegistry.end());

    switch (Action) {
    case WinDragAction::Begin: {
        if (SubStreamExists) {
            return IterSubStreams->second;
        }

        if (Ctx.RequestSubStream && TargetDevice != DeviceMap::C0) {
            SubStreamID NewSubStreamID =
                Ctx.RequestSubStream(TargetDevice, FeatureTypes::WindowLink);
            if (NewSubStreamID != 0) {
                if (StartWindowCaptureStream(Ctx, NewSubStreamID, TargetDevice, Hwnd) == 0) {
                    Logger::log(
                        "Capture stream failed to start for SubStreamID={:d}", NewSubStreamID
                    );
                    if (Ctx.ReleaseSubStream) {
                        Ctx.ReleaseSubStream(TargetDevice, NewSubStreamID, true);
                    }
                    return 0;
                }
                return NewSubStreamID;
            } else {
                Logger::log(
                    "SubStream request for Capture Stream returned 0 for DeviceID={:d}",
                    static_cast<int>(TargetDevice)
                );
            }
        } else {
            Logger::log(
                "Requested SubStream for Window Capture Stream returned null or "
                "TargetDevice is C0 (RequestSubStream={}, TargetDevice={:d})",
                (Ctx.RequestSubStream != nullptr),
                static_cast<int>(TargetDevice)
            );
        }
        return 0;
    }

    case WinDragAction::Move:
        break;

    case WinDragAction::Drop:
        if (SubStreamExists) {
            CenterWindowOnDesktop(Hwnd);
            return IterSubStreams->second;
        }
        return 0;

    case WinDragAction::Cancel: {
        if (SubStreamExists) {
            SubStreamID TargetSubStream = IterSubStreams->second;

            StopWindowCaptureStream(Ctx, TargetSubStream);

            if (Ctx.ReleaseSubStream && TargetDevice != DeviceMap::C0) {
                Ctx.ReleaseSubStream(TargetDevice, TargetSubStream, true);
            }
        }
        return 0;
    }

    default:
        return 0;
    }
}

bool SystemWindowLink::RecallWindow(SystemLinkContext& Ctx, HWND SourceHwnd, bool CenterOnDesktop)
{
    if (!SourceHwnd || !IsWindow(SourceHwnd)) {
        return false;
    }

    auto IterSub = Hwnd2SubStreamRegistry.find(SourceHwnd);
    if (IterSub == Hwnd2SubStreamRegistry.end()) {
        return false;
    }

    SubStreamID StreamID     = IterSub->second;
    DeviceMap   TargetDevice = DeviceMap::C0;
    auto        IterDevice   = SubStream2DeviceRegistry.find(StreamID);
    if (IterDevice != SubStream2DeviceRegistry.end()) {
        TargetDevice = IterDevice->second;
    }

    if (TargetDevice != DeviceMap::C0) {
        auto* NetSession = Ctx.Router.GetWindowSession(TargetDevice);
        if (NetSession) {
            StreamPackets::TransmitDragPacket(
                NetSession, WinDragAction::Cancel, StreamID, TargetDevice, 0, 0, 0, 0
            );
        }
    }

    StopWindowCaptureStream(Ctx, StreamID);

    if (Ctx.ReleaseSubStream && TargetDevice != DeviceMap::C0) {
        Ctx.ReleaseSubStream(TargetDevice, StreamID, true);
    }

    ShowWindow(SourceHwnd, SW_RESTORE);
    if (CenterOnDesktop) {
        CenterWindowOnDesktop(SourceHwnd);
    }
    EnsureWindowFocus(SourceHwnd);

    Logger::log(
        "Recalled window HWND={:p} (SubStreamID={:d}, Center={}) to Host",
        static_cast<void*>(SourceHwnd),
        StreamID,
        CenterOnDesktop
    );

    DragSession = {};
    return true;
}

size_t SystemWindowLink::RecallAllStreamingWindows(SystemLinkContext& Ctx)
{
    if (Hwnd2SubStreamRegistry.empty()) {
        return 0;
    }

    std::vector<HWND> WindowsToRecall;
    WindowsToRecall.reserve(Hwnd2SubStreamRegistry.size());
    for (const auto& [Hwnd, SubId] : Hwnd2SubStreamRegistry) {
        WindowsToRecall.push_back(Hwnd);
    }

    size_t RecalledCount = 0;
    for (HWND Hwnd : WindowsToRecall) {
        if (RecallWindow(Ctx, Hwnd)) {
            RecalledCount++;
        }
    }

    Logger::log("Recalled {:d} streaming window(s) back to Host", RecalledCount);
    return RecalledCount;
}

void SystemWindowLink::HandleWindowDragMove(
    SystemLinkContext& Ctx, HWND Hwnd, const RECT& WindowPos, POINT CursorPos
)
{
    if (DragSession.Phase == DragPhase::Idle || DragSession.Phase == DragPhase::LocalHover) {
        OnLocalDragStart(Hwnd, WindowPos, CursorPos);
    }

    DragSession.LastPos = WindowPos;

    DragSession.ResW = Ctx.Router.ResWidth.load(std::memory_order_relaxed);
    DragSession.ResH = Ctx.Router.ResHeight.load(std::memory_order_relaxed);
    if (DragSession.ResW == 0)
        DragSession.ResW = GetSystemMetrics(SM_CXSCREEN);
    if (DragSession.ResH == 0)
        DragSession.ResH = GetSystemMetrics(SM_CYSCREEN);

    DeviceMap ActiveEdge = Ctx.SpatialRouter.EvaluateWindow(WindowPos);

    OmniNetSession<OmniMTU>* NetSession =
        (ActiveEdge != DeviceMap::C0) ? Ctx.Router.GetWindowSession(ActiveEdge) : nullptr;

    const bool ModifierHeld = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                              ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);

    if (DragSession.Phase == DragPhase::LocalHover && !ModifierHeld) {
        ActiveEdge = DeviceMap::C0;
        NetSession = nullptr;
    }

    const int WindowWidth  = WindowPos.right - WindowPos.left;
    const int WindowHeight = WindowPos.bottom - WindowPos.top;

    if (ActiveEdge != DeviceMap::C0 && NetSession) {
        if (DragSession.Phase == DragPhase::LocalHover) {
            OnEdgeCrossBegin(Ctx, Hwnd, ActiveEdge, WindowPos);
        } else if (DragSession.Phase == DragPhase::Crossing) {
            int16_t RatioX = 0;
            int16_t RatioY = 0;
            OmniSpatialRouter::EncodeEdgePosition(
                ActiveEdge, WindowPos, DragSession.ResW, DragSession.ResH, RatioX, RatioY
            );

            StreamPackets::TransmitDragPacket(
                NetSession,
                WinDragAction::Move,
                DragSession.StreamID,
                ActiveEdge,
                RatioX,
                RatioY,
                static_cast<uint16_t>(WindowWidth),
                static_cast<uint16_t>(WindowHeight),
                static_cast<int16_t>(DragSession.GripX),
                static_cast<int16_t>(DragSession.GripY)
            );
        }
    } else if (DragSession.Phase == DragPhase::Crossing) {
        OnCrossCancel(Ctx, Hwnd);
    }
}

void SystemWindowLink::HandleWindowDragEnd(SystemLinkContext& Ctx, HWND Hwnd)
{
    if (DragSession.Phase == DragPhase::Crossing) {
        OnRemoteDrop(Ctx, Hwnd);
    }

    const bool WasReturning = (DragSession.Phase == DragPhase::Returning);
    DragSession             = {};

    if (WasReturning) {
        RecallWindow(Ctx, Hwnd, false);
    }
}

OmniNet::PoolConfig SystemWindowLink::SetState(
    SystemLinkContext& Ctx,
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            if (SubStreamID == 0) {
                if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                    auto& Instance = Ctx.ActiveInstances->at(DeviceID);
                    Ctx.Router.RegisterWindowSession(DeviceID, Instance.InstanceSession.get());
                } else {
                    Logger::log(
                        "Window Link failed to activate, DeviceID={:d} not found in "
                        "ActiveInstances",
                        static_cast<int>(DeviceID)
                    );
                }

                DragLink.WindowExclusionCheck = [this](HWND Hwnd) {
                    return Hwnd2SubStreamRegistry.contains(Hwnd);
                };
                DragLink.OnDragMove =
                    [this, Ctx](HWND Hwnd, const RECT& WindowPos, POINT CursorPos) mutable {
                        HandleWindowDragMove(Ctx, Hwnd, WindowPos, CursorPos);
                    };
                DragLink.OnDragEnd = [this, Ctx](HWND Hwnd) mutable {
                    HandleWindowDragEnd(Ctx, Hwnd);
                };
                DragLink.WindowMoveListener(true);

                Logger::log("WindowLink enabled for DeviceID {:d}", static_cast<int>(DeviceID));
            }
        } else {
            if (SubStreamID == 0) {
                Ctx.Router.UnregisterWindowSession(DeviceID);

                if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                    auto& Instance = Ctx.ActiveInstances->at(DeviceID);
                    auto  Streams  = Instance.GetSubStreams(FeatureTypes::WindowLink);
                    for (uint16_t StreamID : Streams) {
                        StopWindowCaptureStream(Ctx, StreamID);
                    }
                }

                if (Ctx.Router.GetWindowSessionCount() == 0) {
                    DragLink.WindowMoveListener(false);
                    DragLink.WindowExclusionCheck = nullptr;
                    DragLink.OnDragMove           = nullptr;
                    DragLink.OnDragEnd            = nullptr;
                }
                Logger::log("WindowLink disabled for DeviceID {:d}", static_cast<int>(DeviceID));
            } else {
                StopWindowCaptureStream(Ctx, SubStreamID);
            }
        }
    } else {
        if (Action == FeatureAction::Activate) {
            Logger::log(
                "WindowLink capability activated (Inbound) for device {:d}",
                static_cast<int>(DeviceID)
            );
        } else {
            if (SubStreamID != 0) {
                WindowStreamEntries.erase(SubStreamID);
                Ctx.StreamSessions.DestroyStreamRenderWindow(SubStreamID);
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance = Ctx.ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::WindowLink);
                for (uint16_t StreamID : Streams) {
                    WindowStreamEntries.erase(StreamID);
                    Ctx.StreamSessions.DestroyStreamRenderWindow(StreamID);
                }
            }
        }
    }
    return OmniNet::PoolConfig{};
}
