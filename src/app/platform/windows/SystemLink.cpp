#include "SystemLink.h"
#include "OmniLogger.hpp"

#include <dwmapi.h>

OmniSystemLink::OmniSystemLink(OmniGraphicsContext& GraphicsContext)
    : GraphicsContext(GraphicsContext)
{
}

OmniSystemLink::~OmniSystemLink() {}

void OmniSystemLink::SetupSystemLink(HINSTANCE hInstance_, int nCmdShow_, HWND WindowID_)
{
    hInstance                           = hInstance_;
    nCmdShow                            = nCmdShow_;
    WindowID                            = WindowID_;
    ClipboardLinkModule.ActiveInstances = ActiveInstances;
    InputLinkModule.Setup(WindowID_);
    OmniRouter.SetResolution(GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    Logger::log(
        "SetupSystemLink: Initialized OmniRouter resolution to {}x{}",
        GetSystemMetrics(SM_CXSCREEN),
        GetSystemMetrics(SM_CYSCREEN)
    );
}

void OmniSystemLink::HandleStreamWindowResize(const OmniWinResizePacket Packet)
{
    const SubStreamID StreamID  = Packet.WindowKey;
    const uint32_t    NewWidth  = Packet.Width;
    const uint32_t    NewHeight = Packet.Height;

    if (NewWidth == 0 || NewHeight == 0) {
        return;
    }

    auto IterSourceWindows = WindowLinkModule.SubStream2HwndRegistry.find(StreamID);
    if (IterSourceWindows != WindowLinkModule.SubStream2HwndRegistry.end()) {
        HWND SourceHwnd = IterSourceWindows->second;

        if (IsWindow(SourceHwnd)) {
            RECT CurrentRect{};
            if (GetWindowRect(SourceHwnd, &CurrentRect)) {
                uint32_t CurrentWidth = static_cast<uint32_t>(CurrentRect.right - CurrentRect.left);
                uint32_t CurrentHeight =
                    static_cast<uint32_t>(CurrentRect.bottom - CurrentRect.top);
                if (CurrentWidth == NewWidth && CurrentHeight == NewHeight) {
                    return;
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
        return;
    }

    CaptureStreams.HandleStreamWindowResize(Packet);
}

static void EnsureWindowFocusLocal(HWND TargetHwnd)
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

void OmniSystemLink::HandleStreamWindowMouse(const OmniWinMousePacket& Packet)
{
    auto IterStreamHwnd =
        WindowLinkModule.SubStream2HwndRegistry.find(static_cast<SubStreamID>(Packet.SubStreamID));
    if (IterStreamHwnd != WindowLinkModule.SubStream2HwndRegistry.end() &&
        IsWindow(IterStreamHwnd->second)) {
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
            return;

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
                        const SubStreamID StreamID = static_cast<SubStreamID>(Packet.SubStreamID);
                        auto IterDevice = WindowLinkModule.SubStream2DeviceRegistry.find(StreamID);
                        if (IterDevice != WindowLinkModule.SubStream2DeviceRegistry.end()) {
                            DeviceMap TargetDevice = IterDevice->second;
                            auto*     NetSession   = OmniRouter.GetWindowSession(TargetDevice);
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
                                return;
                            }
                        }
                    }
                }
            }
        }

        constexpr uint16_t MOUSE_DOWN_MASK = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_RIGHTDOWN |
                                             MOUSEEVENTF_MIDDLEDOWN | MOUSEEVENTF_XDOWN;
        if (Packet.Buttons & MOUSE_DOWN_MASK) {
            EnsureWindowFocusLocal(TargetHwnd);
        }

        if (Packet.Buttons | Packet.Wheel) {
            INPUT ButtonInput{};
            ButtonInput.type           = INPUT_MOUSE;
            ButtonInput.mi.mouseData   = Packet.Wheel;
            ButtonInput.mi.dwFlags     = Packet.Buttons;
            ButtonInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
            SendInput(1, &ButtonInput, sizeof(INPUT));
        }
        return;
    }

    CaptureStreams.HandleStreamWindowMouse(Packet);
}

void OmniSystemLink::HandleStreamWindowKey(const OmniWinKeyPacket& Packet)
{
    auto IterStreamHwnd =
        WindowLinkModule.SubStream2HwndRegistry.find(static_cast<SubStreamID>(Packet.SubStreamID));
    if (IterStreamHwnd != WindowLinkModule.SubStream2HwndRegistry.end() &&
        IsWindow(IterStreamHwnd->second)) {
        EnsureWindowFocusLocal(IterStreamHwnd->second);
    }

    CaptureStreams.HandleStreamWindowKey(Packet);
}
