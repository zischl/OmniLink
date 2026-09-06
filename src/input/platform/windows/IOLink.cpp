#include "IOLink.h"
#include "SessionHandler.h"
#include "system_probe_impl.h"

IOLinkContext* OmniIOShield::IOContext = nullptr;

OmniIOShield::OmniIOShield(IOLinkContext& Ctx) : IOCtx(Ctx)
{
    IOContext = &Ctx;
}

void OmniIOShield::InvokeInputFilter()
{
    if (!KeyboardBlock) {
        KeyboardBlock = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(NULL), 0);
    }
    if (!MouseBlock) {
        MouseBlock = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, GetModuleHandle(NULL), 0);
    }

    if (!KeyboardBlock || !MouseBlock)
        std::cout << "Failed to install input hooks.\n";
}

void OmniIOShield::ReleaseInputFilter()
{
    if (KeyboardBlock) {
        UnhookWindowsHookEx(KeyboardBlock);
        KeyboardBlock = NULL;
    }
    if (MouseBlock) {
        UnhookWindowsHookEx(MouseBlock);
        MouseBlock = NULL;
    }
}

LRESULT OmniIOShield::KeyboardProc(int NCode, WPARAM WParam, LPARAM LParam)
{
    if (NCode < 0)
        return CallNextHookEx(nullptr, NCode, WParam, LParam);

    if (IOContext && LParam) {
        KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(LParam);

        if (pKey->dwExtraInfo == OMNI_INPUT_COOKIE) {
            return CallNextHookEx(nullptr, NCode, WParam, LParam);
        }

        if ((GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_MENU) & 0x8000) &&
            (pKey->vkCode == '1' || pKey->vkCode == VK_NUMPAD1)) {
            IOContext->DeactivateEdge();
            return 1;
        }

        if (IOContext->InputLocked.load(std::memory_order_acquire)) {
            auto* NetSession = IOContext->ActiveNetSession.load(std::memory_order_acquire);
            if (NetSession) {
                OmniNet::OmniHeader Header;
                Header.Target     = 0;
                Header.PacketType = OmniNet::PacketType::ProcKey;
                Header.Flags      = 0;

                OmniKeyPacket KeyPacket = {};
                KeyPacket.VkCode        = static_cast<uint16_t>(pKey->vkCode);
                KeyPacket.ScanCode      = static_cast<uint16_t>(pKey->scanCode);
                KeyPacket.Flags         = 0;

                if (pKey->scanCode != 0) {
                    KeyPacket.Flags |= KEYEVENTF_SCANCODE;
                }

                if (pKey->flags & LLKHF_EXTENDED) {
                    KeyPacket.Flags |= KEYEVENTF_EXTENDEDKEY;
                }

                if (pKey->flags & LLKHF_UP) {
                    KeyPacket.Flags |= KEYEVENTF_KEYUP;
                }

                NetSession->SessionSend(
                    reinterpret_cast<CHAR*>(&KeyPacket), sizeof(OmniKeyPacket), Header
                );
            }
            return 1;
        }
    }

    return CallNextHookEx(nullptr, NCode, WParam, LParam);
}

LRESULT OmniIOShield::MouseProc(int NCode, WPARAM WParam, LPARAM LParam)
{
    if (NCode < 0)
        return CallNextHookEx(nullptr, NCode, WParam, LParam);

    if (LParam) {
        MSLLHOOKSTRUCT* pMouse = reinterpret_cast<MSLLHOOKSTRUCT*>(LParam);
        if (pMouse->dwExtraInfo == OMNI_INPUT_COOKIE) {
            return CallNextHookEx(nullptr, NCode, WParam, LParam);
        }
    }

    return (IOContext && IOContext->InputLocked.load(std::memory_order_acquire))
               ? 1
               : CallNextHookEx(nullptr, NCode, WParam, LParam);
}

OmniIOCap::OmniIOCap(IOLinkContext& Ctx) : IOCtx(Ctx)
{
    POINT Pos = {};
    GetCursorPos(&Pos);
    MouseX = Pos.x;
    MouseY = Pos.y;

    RawInputSize = 48;

    Device::MonitorRes MonRes = Device::GetMonitorResolution();
    IOCtx.ResHeight           = MonRes.Height;
    IOCtx.ResWidth            = MonRes.Width;

    FocusEventListener(true);
}

OmniIOCap::~OmniIOCap()
{
    FocusEventListener(false);
    WindowMoveListener(false);
    StopEdgeProbe();
}

void OmniIOCap::FocusEventListener(bool State)
{
    if (WinFocusHook == NULL && State == true) {
        WinFocusHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND,
            EVENT_SYSTEM_CAPTUREEND,
            NULL,
            WinFocusEventProc,
            0,
            0,
            WINEVENT_OUTOFCONTEXT
        );
    } else if (WinFocusHook != NULL && State == false) {
        UnhookWinEvent(WinFocusHook);
        WinFocusHook = NULL;
    }
}

void CALLBACK OmniIOCap::WinFocusEventProc(
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
    (void)Event;
    (void)Hwnd;
    (void)IDObject;
    (void)IDChild;
    (void)IDEventThread;
    (void)DWMSEventTime;

    CURSORINFO CursorInfo = {sizeof(CURSORINFO)};
    if (GetCursorInfo(&CursorInfo)) {
        OmniSynth::GameMode.store((CursorInfo.flags == 0), std::memory_order_relaxed);
    }
}

void OmniIOCap::WindowMoveListener(bool State)
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

void CALLBACK OmniIOCap::WinMvEventProc(
    HWINEVENTHOOK HWinEventHook,
    DWORD         Event,
    HWND          Hwnd,
    LONG          IDObject,
    LONG          IDChild,
    DWORD         IDEventThread,
    DWORD         DWMSEventTime
)
{
    static std::atomic_bool EventStatus{false};

    switch (Event) {
    case EVENT_SYSTEM_MOVESIZESTART: {
        EventStatus.store(true);
        std::thread([Hwnd]() {
            HWND Hwnd_ = Hwnd;
            RECT Pos   = {};
            while (EventStatus.load()) {
                GetWindowRect(Hwnd_, &Pos);
                OutputDebugStringA((std::to_string(Pos.right) + "\n").c_str());
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
        }).detach();
    } break;

    case EVENT_SYSTEM_MOVESIZEEND:
        EventStatus.store(false);
        break;
    }
}

void OmniIOCap::ToggleEdgeProbe(HWND Hwnd)
{
    if (InputLinkStatus.load()) {
        StopEdgeProbe();
    } else {
        CreateEdgeProbe(Hwnd);
    }
}

bool OmniIOCap::GetEdgeProbeState()
{
    return InputLinkStatus.load();
}

void OmniIOCap::StopEdgeProbe()
{
    InputLinkStatus.store(false);
    MouseEventCapStatus.store(false);

    if (ProbeThread.joinable())
        ProbeThread.join();
}

void OmniIOCap::CreateEdgeProbe(HWND Hwnd)
{
    InputLinkStatus.store(true);
    MouseEventCapStatus.store(true);

    ProbeThread = std::thread([this, Hwnd]() {
        HWND  Hwnd_            = Hwnd;
        POINT Pos              = {};
        auto* MouseEventStatus = &MouseEventCapStatus;

        std::cout << "Edge Probe Thread Running\n";

        while (true) {
            // Awaiting edge hit
            while (MouseEventStatus->load()) {
                GetCursorPos(&Pos);
                MouseX = Pos.x;
                MouseY = Pos.y;

                for (auto& [Name, Cond] : Conditions) {
                    if (Cond(MouseX, MouseY)) {
                        IOCtx.ActivateEdge(Name);
                        ActiveEdgeCondition = Name;
                        IOCtx.InputLocked.store(true, std::memory_order_release);

                        uint16_t YRatio =
                            (IOCtx.ResHeight > 0)
                                ? static_cast<uint16_t>(
                                      (static_cast<uint64_t>(Pos.y) << 16) / IOCtx.ResHeight
                                  )
                                : (1 << 15);
                        uint16_t XRatio =
                            (IOCtx.ResWidth > 0)
                                ? static_cast<uint16_t>(
                                      (static_cast<uint64_t>(Pos.x) << 16) / IOCtx.ResWidth
                                  )
                                : (1 << 15);

                        VirtualPosX = Pos.x;
                        VirtualPosY = Pos.y;
                        MouseX      = 0;
                        MouseY      = 0;

                        MouseEventStatus->store(false);
                        ToggleInputCapture(Hwnd_, true);

                        auto* NetSession = IOCtx.ActiveNetSession.load(std::memory_order_acquire);
                        if (NetSession) {
                            OmniNet::OmniHeader Header;
                            Header.Target     = 0;
                            Header.PacketType = OmniNet::PacketType::ProcBoundary;
                            Header.Flags      = 0;

                            OmniBoundaryPacket BoundaryData = {};
                            BoundaryData.Action  = static_cast<uint8_t>(BoundaryAction::Enter);
                            BoundaryData.Edge    = static_cast<uint8_t>(Name);
                            BoundaryData.Y_Ratio = YRatio;
                            BoundaryData.X_Ratio = XRatio;

                            NetSession->SessionSend(
                                reinterpret_cast<CHAR*>(&BoundaryData),
                                sizeof(OmniBoundaryPacket),
                                Header
                            );
                        }

                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            if (!InputLinkStatus.load())
                break;

            MouseEventStatus->store(true);

            // Await until cursor returns home or breakout is triggered
            while (MouseEventStatus->load()) {
                if (!IOCtx.InputLocked.load(std::memory_order_acquire)) {
                    ToggleInputCapture(Hwnd_, false);
                    break;
                }

                bool ReturnState = false;
                switch (ActiveEdgeCondition) {
                case DeviceMap::L1:
                case DeviceMap::LU1:
                case DeviceMap::LD1:
                    ReturnState = (MouseX > 150);
                    break;
                case DeviceMap::R1:
                case DeviceMap::RU1:
                case DeviceMap::RD1:
                    ReturnState = (MouseX < -150);
                    break;
                case DeviceMap::U1:
                    ReturnState = (MouseY > 150);
                    break;
                case DeviceMap::D1:
                    ReturnState = (MouseY < -150);
                    break;
                default:
                    break;
                }

                if (ReturnState) {
                    int targetY = VirtualPosY;
                    int targetX = 2;

                    switch (ActiveEdgeCondition) {
                    case DeviceMap::L1:
                    case DeviceMap::LU1:
                    case DeviceMap::LD1:
                        targetX = 2;
                        break;
                    case DeviceMap::R1:
                    case DeviceMap::RU1:
                    case DeviceMap::RD1:
                        targetX = static_cast<int>(IOCtx.ResWidth - 2);
                        break;
                    case DeviceMap::U1:
                        targetX = VirtualPosX;
                        targetY = static_cast<int>(IOCtx.ResHeight - 2);
                        break;
                    case DeviceMap::D1:
                        targetX = VirtualPosX;
                        targetY = 2;
                        break;
                    default:
                        break;
                    }

                    SetCursorPos(targetX, targetY);

                    IOCtx.InputLocked.store(false, std::memory_order_release);
                    IOCtx.DeactivateEdge();

                    ToggleInputCapture(Hwnd_, false);

                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            if (!InputLinkStatus.load())
                break;
        }
    });
}

void OmniIOCap::AddEdgeCondition(DeviceMap Index)
{
    const uint32_t W = IOCtx.ResWidth;
    const uint32_t H = IOCtx.ResHeight;

    switch (Index) {
    case DeviceMap::L1:
        ConditionManager.Add(Index, [W, H](int X, int Y) {
            return X <= 0 && (Y > 0 && Y < static_cast<int>(H));
        });
        break;

    case DeviceMap::R1:
        ConditionManager.Add(Index, [W, H](int X, int Y) {
            return X >= static_cast<int>(W) && (Y > 0 && Y < static_cast<int>(H));
        });
        break;

    case DeviceMap::U1:
        ConditionManager.Add(Index, [W, H](int X, int Y) {
            return Y <= 0 && (X > 0 && X < static_cast<int>(W));
        });
        break;

    case DeviceMap::D1:
        ConditionManager.Add(Index, [W, H](int X, int Y) {
            return Y >= static_cast<int>(H) && (X > 0 && X < static_cast<int>(W));
        });
        break;

    case DeviceMap::LU1:
        ConditionManager.Add(Index, [](int X, int Y) { return X <= 0 && Y <= 0; });
        break;

    case DeviceMap::RU1:
        ConditionManager.Add(Index, [W](int X, int Y) {
            return X >= static_cast<int>(W) && Y <= 0;
        });
        break;

    case DeviceMap::LD1:
        ConditionManager.Add(Index, [H](int X, int Y) {
            return X <= 0 && Y >= static_cast<int>(H);
        });
        break;

    case DeviceMap::RD1:
        ConditionManager.Add(Index, [W, H](int X, int Y) {
            return X >= static_cast<int>(W) && Y >= static_cast<int>(H);
        });
        break;

    case DeviceMap::C0:
    case DeviceMap::END:
        break;
    }
}

void OmniIOCap::ToggleInputCapture(HWND Hwnd, bool State)
{
    RAWINPUTDEVICE Devices[2];

    Devices[0].usUsage     = HID_USAGE_GENERIC_MOUSE;
    Devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;

    Devices[1].usUsage     = HID_USAGE_GENERIC_KEYBOARD;
    Devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;

    if (State) {
        Devices[0].hwndTarget = Hwnd;
        Devices[0].dwFlags    = RIDEV_INPUTSINK;

        Devices[1].hwndTarget = Hwnd;
        Devices[1].dwFlags    = RIDEV_INPUTSINK;

        RegisterRawInputDevices(Devices, 2, sizeof(Devices[0]));

        InputProc = &OmniIOCap::InputProcCallback;
    } else {
        InputProc = &OmniIOCap::VoidExitCallback;

        Devices[0].hwndTarget = NULL;
        Devices[0].dwFlags    = RIDEV_REMOVE;

        Devices[1].hwndTarget = NULL;
        Devices[1].dwFlags    = RIDEV_REMOVE;

        RegisterRawInputDevices(Devices, 2, sizeof(Devices[0]));
    }
}

void OmniIOCap::InputProcInit(LPARAM& LParam)
{
    InputProcCallback(LParam);
}

void OmniIOCap::InputProcCallback(LPARAM& LParam)
{
    alignas(RAWINPUT) BYTE RawBuffer[sizeof(RAWINPUT)] = {};
    UINT                   Size                        = sizeof(RawBuffer);
    UINT                   Result =
        GetRawInputData((HRAWINPUT)LParam, RID_INPUT, RawBuffer, &Size, sizeof(RAWINPUTHEADER));
    if (Result == (UINT)-1 || Result == 0)
        return;

    RAWINPUT* Input = reinterpret_cast<RAWINPUT*>(RawBuffer);

    auto* NetSession = IOCtx.ActiveNetSession.load(std::memory_order_acquire);
    if (!NetSession)
        return;

    if (Input->header.dwType == RIM_TYPEMOUSE) {
        LONG dX = Input->data.mouse.lLastX;
        LONG dY = Input->data.mouse.lLastY;

        if (dX == 0 && dY == 0 && Input->data.mouse.usButtonFlags == 0)
            return;

        VirtualPosX = std::clamp(
            VirtualPosX + static_cast<int>(dX), 0, static_cast<int>(IOCtx.ResWidth - 1)
        );
        VirtualPosY = std::clamp(
            VirtualPosY + static_cast<int>(dY), 0, static_cast<int>(IOCtx.ResHeight - 1)
        );
        MouseX += dX;
        MouseY += dY;

        OmniNet::OmniHeader Header;
        Header.Target     = 0;
        Header.PacketType = OmniNet::PacketType::ProcMouse;
        Header.Flags      = 0;

        OmniMousePacket Packet = {};
        Packet.dX              = dX;
        Packet.dY              = dY;
        Packet.Flags           = OMNI_MOUSE_RELATIVE;

        if (Input->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) {
            Packet.Buttons |= MOUSEEVENTF_WHEEL;
            Packet.Wheel = static_cast<SHORT>(Input->data.mouse.usButtonData);
        } else if (Input->data.mouse.usButtonFlags & RI_MOUSE_HWHEEL) {
            Packet.Buttons |= MOUSEEVENTF_HWHEEL;
            Packet.Wheel = static_cast<SHORT>(Input->data.mouse.usButtonData);
        }

        if (Input->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
            Packet.Buttons |= MOUSEEVENTF_LEFTDOWN;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
            Packet.Buttons |= MOUSEEVENTF_LEFTUP;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
            Packet.Buttons |= MOUSEEVENTF_RIGHTDOWN;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
            Packet.Buttons |= MOUSEEVENTF_RIGHTUP;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
            Packet.Buttons |= MOUSEEVENTF_MIDDLEDOWN;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
            Packet.Buttons |= MOUSEEVENTF_MIDDLEUP;

        NetSession->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniMousePacket), Header);
    }
}

void OmniIOCap::VoidExitCallback(LPARAM& LParam)
{
    (void)LParam;
}

namespace OmniSynth {
std::atomic<bool> GameMode{false};

void ProcBoundary(const OmniBoundaryPacket& Packet)
{
    if (Packet.Action == static_cast<uint8_t>(BoundaryAction::Enter)) {
        Device::MonitorRes Res = Device::GetMonitorResolution();

        int TargetY =
            (Res.Height > 0)
                ? static_cast<int>((static_cast<uint64_t>(Packet.Y_Ratio) * (Res.Height - 1)) >> 16)
                : static_cast<int>(Res.Height >> 1);

        int TargetX =
            (Res.Width > 0)
                ? static_cast<int>((static_cast<uint64_t>(Packet.X_Ratio) * (Res.Width - 1)) >> 16)
                : static_cast<int>(Res.Width >> 1);

        DeviceMap Edge = static_cast<DeviceMap>(Packet.Edge);
        switch (Edge) {
        case DeviceMap::L1:
        case DeviceMap::LU1:
        case DeviceMap::LD1:
            TargetX = static_cast<int>(Res.Width - 2);
            break;
        case DeviceMap::R1:
        case DeviceMap::RU1:
        case DeviceMap::RD1:
            TargetX = 2;
            break;
        case DeviceMap::U1:
            TargetY = static_cast<int>(Res.Height - 2);
            break;
        case DeviceMap::D1:
            TargetY = 2;
            break;
        default:
            break;
        }

        SetCursorPos(TargetX, TargetY);
    } else if (Packet.Action == static_cast<uint8_t>(BoundaryAction::Return)) {
        auto* Ctx = OmniIOShield::GetContext();
        if (Ctx) {
            Ctx->DeactivateEdge();
        }

        Device::MonitorRes Res = Device::GetMonitorResolution();
        int                TargetY =
            (Res.Height > 0)
                ? static_cast<int>((static_cast<uint64_t>(Packet.Y_Ratio) * (Res.Height - 1)) >> 16)
                : static_cast<int>(Res.Height >> 1);

        DeviceMap Edge    = static_cast<DeviceMap>(Packet.Edge);
        int       TargetX = 2;
        if (Edge == DeviceMap::L1 || Edge == DeviceMap::LU1 || Edge == DeviceMap::LD1) {
            TargetX = 2;
        } else if (Edge == DeviceMap::R1 || Edge == DeviceMap::RU1 || Edge == DeviceMap::RD1) {
            TargetX = static_cast<int>(Res.Width - 2);
        }

        SetCursorPos(TargetX, TargetY);
    }
}

void ProcMouse(const OmniMousePacket& Packet)
{
    if (Packet.Flags & OMNI_MOUSE_ABSOLUTE) {
        INPUT MouseInput          = {0};
        MouseInput.type           = INPUT_MOUSE;
        MouseInput.mi.dx          = Packet.dX;
        MouseInput.mi.dy          = Packet.dY;
        MouseInput.mi.mouseData   = Packet.Wheel;
        MouseInput.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | Packet.Buttons;
        MouseInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
        SendInput(1, &MouseInput, sizeof(INPUT));
        return;
    }

    if (GameMode.load(std::memory_order_relaxed)) {
        INPUT MouseInput        = {0};
        MouseInput.type         = INPUT_MOUSE;
        MouseInput.mi.dx        = Packet.dX;
        MouseInput.mi.dy        = Packet.dY;
        MouseInput.mi.mouseData = Packet.Wheel;
        MouseInput.mi.dwFlags   = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE | Packet.Buttons;
        MouseInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
        SendInput(1, &MouseInput, sizeof(INPUT));
        return;
    }

    if (Packet.dX != 0 || Packet.dY != 0) {
        POINT pt = {};
        GetCursorPos(&pt);
        SetCursorPos(pt.x + Packet.dX, pt.y + Packet.dY);
    }

    if (Packet.Buttons != 0 || Packet.Wheel != 0) {
        INPUT BtnInput        = {0};
        BtnInput.type         = INPUT_MOUSE;
        BtnInput.mi.mouseData = Packet.Wheel;
        BtnInput.mi.dwFlags   = Packet.Buttons;
        BtnInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
        SendInput(1, &BtnInput, sizeof(INPUT));
    }
}

void ProcMouse(int X, int Y)
{
    SetCursorPos(X, Y);
}

void ProcInput(INPUT& Input)
{
    if (Input.type == INPUT_MOUSE) {
        if (Input.mi.dwFlags & MOUSEEVENTF_ABSOLUTE) {
            SendInput(1, &Input, sizeof(INPUT));
        } else {
            if (Input.mi.dx != 0 || Input.mi.dy != 0) {
                POINT pt = {};
                GetCursorPos(&pt);
                SetCursorPos(pt.x + Input.mi.dx, pt.y + Input.mi.dy);
            }

            DWORD btnFlags = Input.mi.dwFlags & ~MOUSEEVENTF_MOVE;
            if (btnFlags != 0) {
                INPUT btnInput      = Input;
                btnInput.mi.dx      = 0;
                btnInput.mi.dy      = 0;
                btnInput.mi.dwFlags = btnFlags;
                SendInput(1, &btnInput, sizeof(INPUT));
            }
        }
    } else {
        SendInput(1, &Input, sizeof(INPUT));
    }
}

void ProcKey(const OmniKeyPacket& Packet)
{
    INPUT KB          = {};
    KB.type           = INPUT_KEYBOARD;
    KB.ki.wVk         = Packet.VkCode;
    KB.ki.wScan       = Packet.ScanCode;
    KB.ki.dwFlags     = Packet.Flags;
    KB.ki.dwExtraInfo = OMNI_INPUT_COOKIE;

    SendInput(1, &KB, sizeof(INPUT));
}

void ProcKey(INPUT& Input)
{
    SendInput(1, &Input, sizeof(INPUT));
}

void ProcKey(KeyData& Input)
{
    INPUT KB      = {};
    KB.type       = INPUT_KEYBOARD;
    KB.ki.wVk     = 0;
    KB.ki.wScan   = Input.MakeCode;
    KB.ki.dwFlags = KEYEVENTF_SCANCODE;

    if (Input.Flags & RI_KEY_BREAK)
        KB.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (Input.Flags & RI_KEY_E0)
        KB.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

    SendInput(1, &KB, sizeof(KB));
}
} // namespace OmniSynth
