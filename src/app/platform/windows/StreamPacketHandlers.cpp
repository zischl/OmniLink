#include "CaptureStreams.hpp"
#include "IOLinkContext.hpp"
#include "OmniRouterContext.hpp"
#include "SpatialRouter.hpp"
#include "WindowLink.hpp"

#include <dwmapi.h>

// Processes Window Drag Events, routing to Source or Stream Window drag handler
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

// Handles drag events on the Host for the captured application window of a given DeviceID.
// Excpects normalized ratios to 0-65535 range for target X and Y coords.
// why ? it's more efficient that way and not dependant on fragile resolution shit.
void SystemWindowLink::HandleStreamSourceDrag(
    SystemLinkContext& Ctx, const OmniWinDragPacket& Packet, DeviceMap DeviceID, HWND SourceHwnd
)
{
    if (!SourceHwnd || !IsWindow(SourceHwnd)) {
        return;
    }

    switch (Packet.Action) {
    case WinDragAction::Move: {
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

        OnReturnBegin(DeviceID, SourceHwnd, TargetX, TargetY);
        break;
    }
    case WinDragAction::Drop: {
        OnReturnDrop(DeviceID, SourceHwnd);
        break;
    }
    case WinDragAction::Cancel: {
        RecallWindow(Ctx, SourceHwnd);
        break;
    }
    default:
        break;
    }
}

// Handles drag events on the Client for the stream render window of a given DeviceID
// Again... Excpects normalized ratios to 0-65535 range for target X and Y coords
// why ? it's more efficient that way and not dependant on fragile resolution shit.
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
        StreamWindow* Window = Ctx.CaptureStreams.FindStreamWindow(Packet.WindowID);
        if (!Window) {
            OmniNet::PoolConfig PoolConfig = SetupClientStreamRenderWindow(
                Ctx,
                Packet.WindowID,
                DeviceID,
                Packet.WindowWidth,
                Packet.WindowHeight,
                static_cast<int16_t>(TargetX),
                static_cast<int16_t>(TargetY)
            );
            if (Ctx.ConfigureSubStream && PoolConfig.Data != nullptr) {
                Ctx.ConfigureSubStream(DeviceID, Packet.WindowID, PoolConfig);
            }
            return;
        }

        HWND TargetHwnd = Window->GetHwnd();
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
        StreamWindow* Window = Ctx.CaptureStreams.FindStreamWindow(Packet.WindowID);
        if (!Window) {
            return;
        }
        HWND TargetHwnd = Window->GetHwnd();
        if (TargetHwnd && IsWindow(TargetHwnd)) {
            SetWindowPos(
                TargetHwnd, NULL, TargetX, TargetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE
            );
        }
        break;
    }
    case WinDragAction::Drop: {
        StreamWindow* Window = Ctx.CaptureStreams.FindStreamWindow(Packet.WindowID);
        if (!Window) {
            return;
        }
        HWND TargetHwnd = Window->GetHwnd();
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
        WindowStreamEntries.erase(Packet.WindowID);
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

// Handles incoming window resize event for source windows on the host
// Meaning... the stream window got resized
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

// Handles incoming window mouse event for source windows on the host
// Meaning... the stream window got some mouse input
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
            const SubStreamID StreamID = static_cast<SubStreamID>(Packet.SubStreamID);
            if (TryInitiateDragFromCaptionTest(Ctx, StreamID, TargetHwnd, CursorPoint, WinRect)) {
                return true;
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

// Handles incoming window key event for source windows on the host
// Meaning... the stream window got some keyboard input
void SystemWindowLink::HandleStreamWindowKey(const OmniWinKeyPacket& Packet)
{
    auto IterStreamHwnd = SubStream2HwndRegistry.find(static_cast<SubStreamID>(Packet.SubStreamID));
    if (IterStreamHwnd != SubStream2HwndRegistry.end() && IsWindow(IterStreamHwnd->second)) {
        EnsureWindowFocus(IterStreamHwnd->second);
    }
}
