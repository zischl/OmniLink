#include "WindowLink.hpp"
#include "CaptureStreams.hpp"
#include "IOLinkContext.hpp"
#include "OmniConfig.hpp"
#include "OmniInstances.hpp"
#include "OmniLogger.hpp"
#include "OmniRouterContext.hpp"
#include "SessionHandler.hpp"
#include "SpatialRouter.hpp"
#include "SubStream.hpp"
#include "SystemLink.hpp"
#include "WinForge.hpp"

#include <dwmapi.h>

static void EnsureWindowFocus(HWND TargetHwnd)
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
        Ctx.CaptureStreams.TransmitWindowResize(NetSession, SubStreamID, DeviceID, Width, Height);
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

    OmniStreamer::StreamID CaptureStreamID = Ctx.CaptureStreams.AddCaptureStream(
        Ctx.GraphicsContext, Entry->SubStream, DeviceID, CaptureMode::WGC_Window, Config
    );
    if (CaptureStreamID == 0) {
        Logger::log(
            "Failed to add a capture stream for HWND={:p}"
            "SubStreamID={:d}",
            static_cast<void*>(Hwnd),
            SubStreamID
        );
        return 0;
    }

    Ctx.CaptureStreams.StreamerIDRegistry[SubStreamID] = CaptureStreamID;
    Hwnd2SubStreamRegistry[Hwnd]                       = SubStreamID;
    SubStream2HwndRegistry[SubStreamID]                = Hwnd;
    SubStream2DeviceRegistry[SubStreamID]              = DeviceID;

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
    auto IterStreamID = Ctx.CaptureStreams.StreamerIDRegistry.find(SubStreamID);
    if (IterStreamID != Ctx.CaptureStreams.StreamerIDRegistry.end()) {
        Ctx.CaptureStreams.Streamer.RemoveStream(IterStreamID->second);
        Ctx.CaptureStreams.StreamerIDRegistry.erase(IterStreamID);
    }

    auto IterHwnd = SubStream2HwndRegistry.find(SubStreamID);
    if (IterHwnd != SubStream2HwndRegistry.end()) {
        Hwnd2SubStreamRegistry.erase(IterHwnd->second);
        SubStream2HwndRegistry.erase(IterHwnd);
    }

    SubStream2DeviceRegistry.erase(SubStreamID);

    Logger::log("WindowCapture stopped for SubStreamID={:d}", SubStreamID);
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
        // This'll never happen for now
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

void SystemWindowLink::HandleStreamWindowDrag(
    SystemLinkContext& Ctx, const OmniWinDragPacket& Packet, DeviceMap SenderDevice
)
{
    DeviceMap DeviceID = (SenderDevice != DeviceMap::C0) ? SenderDevice : Packet.Edge;

    auto IterSource = SubStream2HwndRegistry.find(Packet.WindowID);
    if (IterSource != SubStream2HwndRegistry.end() && IterSource->second) {
        HandleStreamSourceDrag(Ctx, Packet, DeviceID, IterSource->second);
    } else {
        HandleStreamClientDrag(Ctx, Packet, DeviceID);
    }
}

void SystemWindowLink::HandleStreamSourceDrag(
    SystemLinkContext& Ctx, const OmniWinDragPacket& Packet, DeviceMap DeviceID, HWND SourceHwnd
)
{
    if (!SourceHwnd || !IsWindow(SourceHwnd)) {
        return;
    }

    switch (Packet.Action) {
    case WinDragAction::Move: {
        ActiveReturningWindows[DeviceID] = SourceHwnd;
        ReturningSourceWindows.insert(SourceHwnd);

        uint32_t LocalResW = Ctx.Router.ResWidth.load(std::memory_order_relaxed);
        uint32_t LocalResH = Ctx.Router.ResHeight.load(std::memory_order_relaxed);
        if (LocalResW == 0)
            LocalResW = GetSystemMetrics(SM_CXSCREEN);
        if (LocalResH == 0)
            LocalResH = GetSystemMetrics(SM_CYSCREEN);

        int TargetX = 0, TargetY = 0;
        OmniSpatialRouter::DecodeEdgePosition(
            Packet.Edge, Packet.WindowX, Packet.WindowY, LocalResW, LocalResH, TargetX, TargetY
        );

        SetWindowPos(
            SourceHwnd,
            HWND_TOP,
            TargetX,
            TargetY,
            0,
            0,
            SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW
        );
        break;
    }
    case WinDragAction::Drop: {
        ActiveReturningWindows.erase(DeviceID);
        ReturningSourceWindows.erase(SourceHwnd);
        CenterWindowOnDesktop(SourceHwnd);
        break;
    }
    case WinDragAction::Cancel: {
        ActiveReturningWindows.erase(DeviceID);
        ReturningSourceWindows.erase(SourceHwnd);
        RecallWindow(Ctx, SourceHwnd);
        break;
    }
    default:
        break;
    }
}

void SystemWindowLink::HandleStreamClientDrag(
    SystemLinkContext& Ctx, const OmniWinDragPacket& Packet, DeviceMap DeviceID
)
{
    uint32_t LocalResW = Ctx.Router.ResWidth.load(std::memory_order_relaxed);
    uint32_t LocalResH = Ctx.Router.ResHeight.load(std::memory_order_relaxed);
    if (LocalResW == 0)
        LocalResW = GetSystemMetrics(SM_CXSCREEN);
    if (LocalResH == 0)
        LocalResH = GetSystemMetrics(SM_CYSCREEN);

    int TargetX = 0, TargetY = 0;
    OmniSpatialRouter::DecodeEdgePosition(
        Packet.Edge, Packet.WindowX, Packet.WindowY, LocalResW, LocalResH, TargetX, TargetY
    );

    switch (Packet.Action) {
    case WinDragAction::Begin: {
        auto IterStreamWindows = Ctx.CaptureStreams.StreamWindowRegistry.find(Packet.WindowID);
        if (IterStreamWindows == Ctx.CaptureStreams.StreamWindowRegistry.end() ||
            !IterStreamWindows->second) {
            OmniNet::PoolConfig PoolConfig = Ctx.CaptureStreams.SetupStreamRenderWindow(
                Ctx,
                Packet.WindowID,
                DeviceID,
                Packet.WindowWidth,
                Packet.WindowHeight,
                static_cast<int16_t>(TargetX),
                static_cast<int16_t>(TargetY),
                SW_SHOW
            );
            if (Ctx.ConfigureSubStream && PoolConfig.Data != nullptr) {
                Ctx.ConfigureSubStream(DeviceID, Packet.WindowID, PoolConfig);
            }
            return;
        }

        StreamWindow* Window     = IterStreamWindows->second;
        HWND          TargetHwnd = Window->GetHwnd();
        if (TargetHwnd && IsWindow(TargetHwnd)) {
            if (Packet.WindowWidth == 0 && Packet.WindowHeight == 0) {
                ReleaseCapture();
                PostMessage(TargetHwnd, WM_SYSCOMMAND, 0xF012, 0);
            } else {
                Window->UpdateDimensions(Packet.WindowWidth, Packet.WindowHeight);
                SetWindowPos(
                    TargetHwnd,
                    NULL,
                    TargetX,
                    TargetY,
                    Packet.WindowWidth,
                    Packet.WindowHeight,
                    SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW
                );
            }
        }
        break;
    }
    case WinDragAction::Move: {
        auto IterStreamWindows = Ctx.CaptureStreams.StreamWindowRegistry.find(Packet.WindowID);
        if (IterStreamWindows == Ctx.CaptureStreams.StreamWindowRegistry.end() ||
            !IterStreamWindows->second) {
            return;
        }
        HWND TargetHwnd = IterStreamWindows->second->GetHwnd();
        if (TargetHwnd && IsWindow(TargetHwnd)) {
            SetWindowPos(
                TargetHwnd, NULL, TargetX, TargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        break;
    }
    case WinDragAction::Drop: {
        auto IterStreamWindow = Ctx.CaptureStreams.StreamWindowRegistry.find(Packet.WindowID);
        if (IterStreamWindow == Ctx.CaptureStreams.StreamWindowRegistry.end() ||
            !IterStreamWindow->second) {
            return;
        }
        StreamWindow* Window     = IterStreamWindow->second;
        HWND          TargetHwnd = Window->GetHwnd();
        if (TargetHwnd && IsWindow(TargetHwnd)) {
            if (Packet.WindowWidth > 0 && Packet.WindowHeight > 0) {
                Window->UpdateDimensions(Packet.WindowWidth, Packet.WindowHeight);
            }
            SetWindowPos(TargetHwnd, NULL, TargetX, TargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            SetForegroundWindow(TargetHwnd);
        }
        break;
    }
    case WinDragAction::Cancel: {
        Ctx.CaptureStreams.DestroyStreamRenderWindow(Packet.WindowID);
        if (Ctx.ReleaseSubStream) {
            Ctx.ReleaseSubStream(DeviceID, Packet.WindowID, false);
        }
        break;
    }
    default:
        break;
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
            OmniWinDragPacket CancelPacket{};
            CancelPacket.Action   = WinDragAction::Cancel;
            CancelPacket.WindowID = StreamID;
            CancelPacket.Edge     = TargetDevice;

            OmniNet::OmniHeader Header;
            Header.Target     = 0;
            Header.PacketType = OmniNet::PacketType::ProcWinDrag;
            Header.Flags      = 0;

            NetSession->SessionSend(
                reinterpret_cast<CHAR*>(&CancelPacket), sizeof(OmniWinDragPacket), Header
            );
        }
    }

    StopWindowCaptureStream(Ctx, StreamID);

    if (Ctx.ReleaseSubStream && TargetDevice != DeviceMap::C0) {
        Ctx.ReleaseSubStream(TargetDevice, StreamID, true);
    }

    if (TargetDevice != DeviceMap::C0) {
        ActiveReturningWindows.erase(TargetDevice);
    }
    ReturningSourceWindows.erase(SourceHwnd);

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
    // Latch grip offset while the window is still local (may update each tick until edge cross)
    if (DragState.SpatialState == EdgeSpatiality::Local) {
        DragState.GripX = CursorPos.x - WindowPos.left;
        DragState.GripY = CursorPos.y - WindowPos.top;
    }

    DragState.LastWindowPos = WindowPos;

    DragState.ResW = Ctx.Router.ResWidth.load(std::memory_order_relaxed);
    DragState.ResH = Ctx.Router.ResHeight.load(std::memory_order_relaxed);
    if (DragState.ResW == 0)
        DragState.ResW = GetSystemMetrics(SM_CXSCREEN);
    if (DragState.ResH == 0)
        DragState.ResH = GetSystemMetrics(SM_CYSCREEN);

    DeviceMap ActiveEdge = Ctx.SpatialRouter.EvaluateWindow(WindowPos);

    OmniNetSession<OmniMTU>* NetSession =
        (ActiveEdge != DeviceMap::C0) ? Ctx.Router.GetWindowSession(ActiveEdge) : nullptr;

    const bool ModifierHeld = ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) ||
                              ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0);

    if (DragState.SpatialState == EdgeSpatiality::Local && !ModifierHeld) {
        ActiveEdge = DeviceMap::C0;
        NetSession = nullptr;
    }

    const int WindowWidth  = WindowPos.right - WindowPos.left;
    const int WindowHeight = WindowPos.bottom - WindowPos.top;

    if (ActiveEdge != DeviceMap::C0 && NetSession) {
        int16_t RatioX = 0;
        int16_t RatioY = 0;
        OmniSpatialRouter::EncodeEdgePosition(
            ActiveEdge, WindowPos, DragState.ResW, DragState.ResH, RatioX, RatioY
        );

        OmniNet::OmniHeader Header;
        Header.Target     = 0;
        Header.PacketType = OmniNet::PacketType::ProcWinDrag;
        Header.Flags      = 0;

        OmniWinDragPacket Packet = {};
        Packet.Edge              = ActiveEdge;
        Packet.WindowX           = RatioX;
        Packet.WindowY           = RatioY;
        Packet.WindowWidth       = static_cast<uint16_t>(WindowWidth);
        Packet.WindowHeight      = static_cast<uint16_t>(WindowHeight);
        Packet.CursorGripX       = static_cast<int16_t>(DragState.GripX);
        Packet.CursorGripY       = static_cast<int16_t>(DragState.GripY);

        if (DragState.SpatialState == EdgeSpatiality::Local) {
            SubStreamID StreamID =
                HandleWindowDragEvent(Ctx, Hwnd, ActiveEdge, WinDragAction::Begin);
            if (StreamID == 0) {
                return;
            }
            DragState.WindowID     = StreamID;
            DragState.ActiveEdge   = ActiveEdge;
            DragState.SpatialState = EdgeSpatiality::InTransit;

            Packet.Action   = WinDragAction::Begin;
            Packet.WindowID = StreamID;
            NetSession->SessionSend(
                reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
            );
        } else {
            Packet.Action   = WinDragAction::Move;
            Packet.WindowID = DragState.WindowID;
            NetSession->SessionSend(
                reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
            );
        }
    } else if (DragState.SpatialState == EdgeSpatiality::InTransit) {
        auto* Session = Ctx.Router.GetWindowSession(DragState.ActiveEdge);
        if (Session) {
            OmniNet::OmniHeader Header;
            Header.Target     = 0;
            Header.PacketType = OmniNet::PacketType::ProcWinDrag;
            Header.Flags      = 0;

            OmniWinDragPacket Packet = {};
            Packet.Action            = WinDragAction::Cancel;
            Packet.Edge              = DragState.ActiveEdge;
            Packet.WindowID          = DragState.WindowID;
            Session->SessionSend(
                reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
            );
        }
        HandleWindowDragEvent(Ctx, Hwnd, DragState.ActiveEdge, WinDragAction::Cancel);
        DragState.WindowID     = 0;
        DragState.ActiveEdge   = DeviceMap::C0;
        DragState.SpatialState = EdgeSpatiality::Local;
    }
}

void SystemWindowLink::HandleWindowDragEnd(SystemLinkContext& Ctx, HWND Hwnd)
{
    if (DragState.SpatialState == EdgeSpatiality::InTransit) {
        RECT FinalPos = DragState.LastWindowPos;
        GetWindowRect(Hwnd, &FinalPos);
        const int WinW = FinalPos.right - FinalPos.left;
        const int WinH = FinalPos.bottom - FinalPos.top;

        int16_t RatioX = 0;
        int16_t RatioY = 0;
        OmniSpatialRouter::EncodeEdgePosition(
            DragState.ActiveEdge, FinalPos, DragState.ResW, DragState.ResH, RatioX, RatioY
        );

        auto* NetSession = Ctx.Router.GetWindowSession(DragState.ActiveEdge);
        if (NetSession) {
            OmniNet::OmniHeader Header;
            Header.Target     = 0;
            Header.PacketType = OmniNet::PacketType::ProcWinDrag;
            Header.Flags      = 0;

            OmniWinDragPacket Packet = {};
            Packet.Action            = WinDragAction::Drop;
            Packet.Edge              = DragState.ActiveEdge;
            Packet.WindowX           = RatioX;
            Packet.WindowY           = RatioY;
            Packet.WindowWidth       = static_cast<uint16_t>(WinW);
            Packet.WindowHeight      = static_cast<uint16_t>(WinH);
            Packet.CursorGripX       = static_cast<int16_t>(DragState.GripX);
            Packet.CursorGripY       = static_cast<int16_t>(DragState.GripY);
            Packet.WindowID          = DragState.WindowID;
            NetSession->SessionSend(
                reinterpret_cast<CHAR*>(&Packet), sizeof(OmniWinDragPacket), Header
            );
        }

        HandleWindowDragEvent(Ctx, Hwnd, DragState.ActiveEdge, WinDragAction::Drop);
    }

    DragState = {};

    if (ReturningSourceWindows.contains(Hwnd)) {
        ReturningSourceWindows.erase(Hwnd);
        RecallWindow(Ctx, Hwnd, false);
    }
}

bool SystemWindowLink::HandleStreamWindowResize(const OmniWinResizePacket& Packet)
{
    const SubStreamID StreamID  = Packet.WindowKey;
    const uint32_t    NewWidth  = Packet.Width;
    const uint32_t    NewHeight = Packet.Height;

    if (NewWidth == 0 || NewHeight == 0) {
        return true;
    }

    auto IterSourceWindows = SubStream2HwndRegistry.find(StreamID);
    if (IterSourceWindows != SubStream2HwndRegistry.end()) {
        HWND SourceHwnd = IterSourceWindows->second;

        if (IsWindow(SourceHwnd)) {
            RECT CurrentRect{};
            if (GetWindowRect(SourceHwnd, &CurrentRect)) {
                uint32_t CurrentWidth = static_cast<uint32_t>(CurrentRect.right - CurrentRect.left);
                uint32_t CurrentHeight =
                    static_cast<uint32_t>(CurrentRect.bottom - CurrentRect.top);
                if (CurrentWidth == NewWidth && CurrentHeight == NewHeight) {
                    return true;
                }
            }
            SetWindowPos(
                SourceHwnd,
                nullptr,
                0,
                0,
                static_cast<int>(NewWidth),
                static_cast<int>(NewHeight),
                SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        return true;
    }

    return false;
}

bool SystemWindowLink::HandleStreamWindowMouse(
    SystemLinkContext& Ctx, const OmniWinMousePacket& Packet
)
{
    auto IterStreamHwnd = SubStream2HwndRegistry.find(static_cast<SubStreamID>(Packet.SubStreamID));
    if (IterStreamHwnd != SubStream2HwndRegistry.end() && IsWindow(IterStreamHwnd->second)) {
        HWND TargetHwnd = IterStreamHwnd->second;

        RECT    WinRect{};
        HRESULT hr = DwmGetWindowAttribute(
            TargetHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &WinRect, sizeof(WinRect)
        );
        if (FAILED(hr)) {
            GetWindowRect(TargetHwnd, &WinRect);
        }

        LONG WinW = WinRect.right - WinRect.left;
        LONG WinH = WinRect.bottom - WinRect.top;
        if (WinW <= 0 || WinH <= 0)
            return true;

        POINT CursorPoint = {
            WinRect.left +
                static_cast<LONG>((static_cast<uint64_t>(Packet.NormalizedX) * WinW) / 65535ULL),
            WinRect.top +
                static_cast<LONG>((static_cast<uint64_t>(Packet.NormalizedY) * WinH) / 65535ULL)
        };

        SetCursorPos(CursorPoint.x, CursorPoint.y);

        if (Packet.Buttons & MOUSEEVENTF_LEFTDOWN) {
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
                        const SubStreamID StreamID   = static_cast<SubStreamID>(Packet.SubStreamID);
                        auto              IterDevice = SubStream2DeviceRegistry.find(StreamID);
                        if (IterDevice != SubStream2DeviceRegistry.end()) {
                            DeviceMap TargetDevice = IterDevice->second;
                            auto*     NetSession   = Ctx.Router.GetWindowSession(TargetDevice);
                            if (NetSession) {
                                OmniWinDragPacket DragPacket{};
                                DragPacket.Action   = WinDragAction::Begin;
                                DragPacket.WindowID = StreamID;
                                DragPacket.Edge     = TargetDevice;

                                OmniNet::OmniHeader Header;
                                Header.Target     = 0;
                                Header.PacketType = OmniNet::PacketType::ProcWinDrag;
                                Header.Flags      = 0;

                                NetSession->SessionSend(
                                    reinterpret_cast<CHAR*>(&DragPacket),
                                    sizeof(OmniWinDragPacket),
                                    Header
                                );
                                return true;
                            }
                        }
                    }
                }
            }
        }

        constexpr uint16_t MOUSE_DOWN_MASK = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_RIGHTDOWN |
                                             MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_XDOWN;
        if (Packet.Buttons & MOUSE_DOWN_MASK) {
            EnsureWindowFocus(TargetHwnd);
        }

        if (Packet.Buttons | Packet.Wheel) {
            INPUT ButtonInput{};
            ButtonInput.type           = INPUT_MOUSE;
            ButtonInput.mi.mouseData   = Packet.Wheel;
            ButtonInput.mi.dwFlags     = Packet.Buttons;
            ButtonInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
            SendInput(1, &ButtonInput, sizeof(INPUT));
        }
        return true;
    }

    return false;
}

void SystemWindowLink::HandleStreamWindowKey(const OmniWinKeyPacket& Packet)
{
    auto IterStreamHwnd = SubStream2HwndRegistry.find(static_cast<SubStreamID>(Packet.SubStreamID));
    if (IterStreamHwnd != SubStream2HwndRegistry.end() && IsWindow(IterStreamHwnd->second)) {
        EnsureWindowFocus(IterStreamHwnd->second);
    }
}
void SystemWindowLink::HandleReturningWindow(DeviceMap DeviceID)
{
    auto IterReturners = ActiveReturningWindows.find(DeviceID);
    if (IterReturners != ActiveReturningWindows.end() && IterReturners->second &&
        IsWindow(IterReturners->second)) {
        HWND ReturningHwnd = IterReturners->second;
        ActiveReturningWindows.erase(IterReturners);
        PostMessage(ReturningHwnd, WM_SYSCOMMAND, 0xF012, 0);
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
            } else {
                StartWindowCaptureStream(
                    Ctx, SubStreamID, DeviceID, reinterpret_cast<HWND>(Context)
                );
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
                Ctx.CaptureStreams.DestroyStreamRenderWindow(SubStreamID);
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance = Ctx.ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::WindowLink);
                for (uint16_t StreamID : Streams) {
                    Ctx.CaptureStreams.DestroyStreamRenderWindow(StreamID);
                }
            }
        }
    }
    return OmniNet::PoolConfig{};
}
