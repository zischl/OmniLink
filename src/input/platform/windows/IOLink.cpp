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
    KeyboardBlock = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    MouseBlock = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);

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
    return (IOContext && IOContext->InputLocked.load(std::memory_order_acquire))
               ? 1
               : CallNextHookEx(nullptr, NCode, WParam, LParam);
}

LRESULT OmniIOShield::MouseProc(int NCode, WPARAM WParam, LPARAM LParam)
{
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
    IOCtx.ResHeight = MonRes.Height;
    IOCtx.ResWidth = MonRes.Width;
}

OmniIOCap::~OmniIOCap()
{
    StopEdgeProbe();
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
    DWORD Event,
    HWND Hwnd,
    LONG IDObject,
    LONG IDChild,
    DWORD IDEventThread,
    DWORD DWMSEventTime
)
{
    static std::atomic_bool EventStatus{false};

    switch (Event) {
    case EVENT_SYSTEM_MOVESIZESTART: {
        EventStatus.store(true);
        std::thread([Hwnd]() {
            HWND Hwnd_ = Hwnd;
            RECT Pos = {};
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
        HWND Hwnd_ = Hwnd;
        POINT Pos = {};
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

                        MouseEventStatus->store(false);
                        ToggleInputCapture(Hwnd_, true);

                        break;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }

            if (!InputLinkStatus.load())
                break;

            MouseEventStatus->store(true);

            // Await until cursor returns home
            while (MouseEventStatus->load()) {
                bool ReturnState = (MouseX < static_cast<int>(IOCtx.ResWidth)) && (MouseX > 0) &&
                                   (MouseY < static_cast<int>(IOCtx.ResHeight)) && (MouseY > 0);

                if (ReturnState) {
                    IOCtx.InputLocked.store(false, std::memory_order_release);
                    IOCtx.DeactivateEdge();

                    ToggleInputCapture(Hwnd_, false);

                    std::this_thread::sleep_for(std::chrono::milliseconds(300));
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(150));
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

    Devices[0].hwndTarget = Hwnd;
    Devices[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    Devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;

    Devices[1].hwndTarget = Hwnd;
    Devices[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    Devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;

    if (State) {
        Devices[0].dwFlags = RIDEV_INPUTSINK;
        Devices[1].dwFlags = RIDEV_INPUTSINK;
        RegisterRawInputDevices(Devices, 2, sizeof(Devices[0]));

        InputProc = &OmniIOCap::InputProcInit;
    } else {
        InputProc = &OmniIOCap::VoidExitCallback;

        Devices[0].dwFlags = RIDEV_REMOVE;
        Devices[1].dwFlags = RIDEV_REMOVE;
        RegisterRawInputDevices(Devices, 2, sizeof(Devices[0]));
    }
}

void OmniIOCap::InputProcInit(LPARAM& LParam)
{
    GetRawInputData((HRAWINPUT)LParam, RID_INPUT, NULL, &RawInputSize, sizeof(RAWINPUTHEADER));

    InputProc = &OmniIOCap::InputProcCallback;

    auto* NetSession = IOCtx.ActiveNetSession.load(std::memory_order_acquire);
    if (!NetSession)
        return;

    OmniNet::OmniHeader Header;
    Header.Target = 0;
    Header.PacketType = OmniNet::PacketType::ProcMouse;
    Header.Flags = 0;

    INPUT MouseInput = {0};
    MouseInput.type = INPUT_MOUSE;
    MouseInput.mi.dx = PointCache[static_cast<uint8_t>(IOCtx.ActiveEdge)].X;
    MouseInput.mi.dy = PointCache[static_cast<uint8_t>(IOCtx.ActiveEdge)].Y;
    MouseInput.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;

    NetSession->SessionSend(reinterpret_cast<CHAR*>(&MouseInput), sizeof(INPUT), Header);
}

void OmniIOCap::InputProcCallback(LPARAM& LParam)
{
    std::vector<BYTE> Buffer(RawInputSize);
    GetRawInputData(
        (HRAWINPUT)LParam, RID_INPUT, Buffer.data(), &RawInputSize, sizeof(RAWINPUTHEADER)
    );
    RAWINPUT* Input = reinterpret_cast<RAWINPUT*>(Buffer.data());

    auto* NetSession = IOCtx.ActiveNetSession.load(std::memory_order_acquire);
    if (!NetSession)
        return;

    if (Input->header.dwType == RIM_TYPEMOUSE) {
        MouseX += Input->data.mouse.lLastX;
        MouseY += Input->data.mouse.lLastY;

        OmniNet::OmniHeader Header;
        Header.Target = 0;
        Header.PacketType = OmniNet::PacketType::ProcMouse;
        Header.Flags = 0;

        INPUT MouseInput = {0};
        MouseInput.type = INPUT_MOUSE;
        MouseInput.mi.dx = Input->data.mouse.lLastX;
        MouseInput.mi.dy = Input->data.mouse.lLastY;
        MouseInput.mi.mouseData = static_cast<SHORT>(Input->data.mouse.usButtonData);
        MouseInput.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;

        if (Input->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
            MouseInput.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
            MouseInput.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
            MouseInput.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
            MouseInput.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_WHEEL)
            MouseInput.mi.dwFlags |= MOUSEEVENTF_WHEEL;
        if (Input->data.mouse.usButtonFlags & RI_MOUSE_HWHEEL)
            MouseInput.mi.dwFlags |= MOUSEEVENTF_HWHEEL;

        NetSession->SessionSend(reinterpret_cast<CHAR*>(&MouseInput), sizeof(INPUT), Header);

    } else if (Input->header.dwType == RIM_TYPEKEYBOARD) {
        OmniNet::OmniHeader Header;
        Header.Target = 0;
        Header.PacketType = OmniNet::PacketType::ProcKey;
        Header.Flags = 0;

        INPUT KBInput = {0};
        KBInput.type = INPUT_KEYBOARD;
        KBInput.ki.wScan = static_cast<SHORT>(Input->data.keyboard.MakeCode);
        KBInput.ki.dwFlags = KEYEVENTF_SCANCODE;

        if (Input->data.keyboard.Flags & RI_KEY_E0)
            KBInput.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        if (Input->data.keyboard.Flags & RI_KEY_BREAK)
            KBInput.ki.dwFlags |= KEYEVENTF_KEYUP;

        NetSession->SessionSend(reinterpret_cast<CHAR*>(&KBInput), sizeof(INPUT), Header);
    }
}

void OmniIOCap::VoidExitCallback(LPARAM& LParam)
{
    (void)LParam;
}

namespace OmniSynth {
void ProcMouse(int X, int Y)
{
    SetCursorPos(X, Y);
}

void ProcInput(INPUT& Input)
{
    SendInput(1, &Input, sizeof(INPUT));
}

void ProcKey(INPUT& Input)
{
    SendInput(1, &Input, sizeof(INPUT));
}

void ProcKey(KeyData& Input)
{
    INPUT KB = {};
    KB.type = INPUT_KEYBOARD;
    KB.ki.wVk = 0;
    KB.ki.wScan = Input.MakeCode;
    KB.ki.dwFlags = KEYEVENTF_SCANCODE;

    if (Input.Flags & RI_KEY_BREAK)
        KB.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (Input.Flags & RI_KEY_E0)
        KB.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;

    SendInput(1, &KB, sizeof(KB));
}
} // namespace OmniSynth
