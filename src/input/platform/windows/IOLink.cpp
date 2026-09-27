#include "IOLink.hpp"
#include "IOLinkContext.hpp"
#include "OmniEnums.hpp"
#include "OmniInstances.h"
#include "OmniRouterContext.hpp"
#include "OmniTypes.hpp"
#include "SessionHandler.hpp"
#include "SessionTypes.hpp"
#include "system_probe_impl.hpp"

#include <atomic>
#include <cstdint>
#include <windef.h>
#include <winuser.h>

InputLinkContext* OmniInputFilter::IOContext = nullptr;

OmniInputFilter::OmniInputFilter(InputLinkContext& Ctx) : IOCtx(Ctx)
{
    IOContext = &Ctx;
}

void OmniInputFilter::InvokeInputFilter()
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

void OmniInputFilter::ReleaseInputFilter()
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

LRESULT OmniInputFilter::KeyboardProc(int NCode, WPARAM WParam, LPARAM LParam)
{
    if (NCode >= 0 && IOContext && IOContext->InputLocked.load(std::memory_order_relaxed))
        [[unlikely]] {
        auto* KeyData = reinterpret_cast<KBDLLHOOKSTRUCT*>(LParam);
        if (KeyData) {
            if (KeyData->dwExtraInfo == OMNI_INPUT_COOKIE) {
                return CallNextHookEx(nullptr, NCode, WParam, LParam);
            }

            if ((GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_MENU) & 0x8000) &&
                (KeyData->vkCode == '1' || KeyData->vkCode == VK_NUMPAD1)) {
                IOContext->DeactivateRemoteEdge();
                return 1;
            }

            auto* NetSession = IOContext->Router.GetSession(
                IOContext->ActiveRemoteEdge.load(std::memory_order_acquire)
            );
            if (NetSession) {
                OmniNet::OmniHeader Header;
                Header.Target     = 0;
                Header.PacketType = OmniNet::PacketType::ProcKey;
                Header.Flags      = 0;

                OmniKeyPacket KeyPacket = {};
                KeyPacket.VkCode        = static_cast<uint16_t>(KeyData->vkCode);
                KeyPacket.ScanCode      = static_cast<uint16_t>(KeyData->scanCode);
                KeyPacket.Flags         = 0;

                if (KeyData->scanCode != 0) {
                    KeyPacket.Flags |= KEYEVENTF_SCANCODE;
                }

                if (KeyData->flags & LLKHF_EXTENDED) {
                    KeyPacket.Flags |= KEYEVENTF_EXTENDEDKEY;
                }

                if (KeyData->flags & LLKHF_UP) {
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

LRESULT OmniInputFilter::MouseProc(int NCode, WPARAM WParam, LPARAM LParam)
{
    if (NCode >= 0 && IOContext && IOContext->InputLocked.load(std::memory_order_relaxed))
        [[unlikely]] {
        auto* MouseData = reinterpret_cast<MSLLHOOKSTRUCT*>(LParam);
        if (!MouseData || MouseData->dwExtraInfo != OMNI_INPUT_COOKIE) {
            return 1;
        }
    }

    return CallNextHookEx(nullptr, NCode, WParam, LParam);
}

OmniInputLink::OmniInputLink(InputLinkContext& Ctx) : IOCtx(Ctx)
{
    POINT Pos = {};
    GetCursorPos(&Pos);
    MouseX = Pos.x;
    MouseY = Pos.y;

    RawInputSize = 48;

    Device::MonitorRes MonRes = Device::GetMonitorResolution();
    IOCtx.Router.SetResolution(MonRes.Width, MonRes.Height);

    FocusEventListener(true);
}

OmniInputLink::~OmniInputLink()
{
    FocusEventListener(false);
    StopEdgeProbe();
}

void OmniInputLink::FocusEventListener(bool State)
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

void CALLBACK OmniInputLink::WinFocusEventProc(
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

uint8_t OmniInputLink::ComputeRelativeSpartialGrid(DeviceMap DeviceID, uint8_t EdgeMask)
{
    uint8_t RelativeEdgeMask = 0;
    while (EdgeMask) {
        DeviceMap RelativeID =
            ComputeRelativeSpartialID(DeviceID, static_cast<DeviceMap>(std::countr_zero(EdgeMask)));
        RelativeEdgeMask |= (1U << RelativeID);
        EdgeMask &= static_cast<uint8_t>(EdgeMask - 1);
    }

    return RelativeEdgeMask;
}

void OmniInputLink::ToggleEdgeProbe()
{
    if (InputLinkStatus.load()) {
        StopEdgeProbe();
    } else {
        CreateEdgeProbe();
    }
}

bool OmniInputLink::GetEdgeProbeState()
{
    return InputLinkStatus.load();
}

void OmniInputLink::AddRelayMask(DeviceMap DeviceID)
{
    ActiveEdgeRelayMask.fetch_or(
        static_cast<uint8_t>(1U << (static_cast<unsigned>(DeviceID) - 1U)),
        std::memory_order_relaxed
    );
}

void OmniInputLink::RemoveRelayMask(DeviceMap DeviceID)
{
    ActiveEdgeRelayMask.fetch_and(
        static_cast<uint8_t>(~(1U << (static_cast<unsigned>(DeviceID) - 1U))),
        std::memory_order_relaxed
    );
}

void OmniInputLink::ResetEdgeRelayMask()
{
    uint8_t ActiveMask = ActiveEdgeRelayMask.load(std::memory_order_release);

    while (ActiveMask) {
        auto DeviceID = static_cast<DeviceMap>(std::countr_zero(ActiveMask) + 1);
        RemoveEdgeCondition(DeviceID);
        ActiveMask &= static_cast<uint8_t>(ActiveMask - 1);
    }

    ActiveEdgeRelayMask.store(0, std::memory_order_relaxed);
}

void OmniInputLink::SetEdgeRelayMask(DeviceMap RemoteID, uint8_t EdgeMask)
{
    ResetEdgeRelayMask();

    DeviceMap ReverseID = ComputeRelativeSpartialID(RemoteID, DeviceMap::C0);

    while (EdgeMask) {
        DeviceMap DeviceID = static_cast<DeviceMap>(std::countr_zero(EdgeMask) + 1);
        DeviceMap RelativeID =
            ComputeRelativeSpartialID(ReverseID, static_cast<DeviceMap>(DeviceID));

        if (RelativeID != DeviceMap::END && RelativeID != DeviceMap::C0) {
            AddRelayMask(RelativeID);
            AddEdgeCondition(RelativeID);
        }

        EdgeMask &= static_cast<uint8_t>(EdgeMask - 1);
    }
}

bool OmniInputLink::GetEdgeRelayState(DeviceMap DeviceID)
{
    return ActiveEdgeRelayMask & (1U << static_cast<uint8_t>(DeviceID - 1));
}

uint8_t OmniInputLink::GetEdgeRelayMask(DeviceMap TargetID)
{
    uint8_t EdgeMask = 0;

    for (auto& [DeviceID, Cond] : Conditions) {
        EdgeMask |= 1 << (static_cast<uint8_t>(DeviceID) - 1);
    }

    return EdgeMask & ~(1U << (static_cast<uint8_t>(TargetID) - 1));
}

void OmniInputLink::StopEdgeProbe()
{
    InputLinkStatus.store(false);
    MouseEventCapStatus.store(false);

    if (ProbeThread.joinable())
        ProbeThread.join();
}

void OmniInputLink::CreateEdgeProbe()
{
    InputLinkStatus.store(true);
    MouseEventCapStatus.store(true);

    ProbeThread = std::thread([this]() {
        HWND  Hwnd_            = CaptureHWND;
        POINT Pos              = {};
        auto* MouseEventStatus = &MouseEventCapStatus;

        std::cout << "Edge Probe Thread Running\n";

        while (true) {
            SetCaptureHIDMode(HIDMON);

            // Awaiting edge hit
            while (MouseEventStatus->load()) {
                GetCursorPos(&Pos);
                MouseX = Pos.x;
                MouseY = Pos.y;

                for (auto& [DeviceID, Cond] : Conditions) {
                    if (Cond(MouseX, MouseY)) {

                        uint16_t YRatio =
                            (IOCtx.Router.ResHeight > 0)
                                ? static_cast<uint16_t>(
                                      (static_cast<uint64_t>(Pos.y) << 16) / IOCtx.Router.ResHeight
                                  )
                                : (1 << 15);
                        uint16_t XRatio =
                            (IOCtx.Router.ResWidth > 0)
                                ? static_cast<uint16_t>(
                                      (static_cast<uint64_t>(Pos.x) << 16) / IOCtx.Router.ResWidth
                                  )
                                : (1 << 15);

                        MouseX = 0;
                        MouseY = 0;

                        MouseEventStatus->store(false);

                        bool  RemoteActive = CursorOwner.load(std::memory_order_acquire) != C0;
                        auto* NetSession   = IOCtx.Router.GetSession(DeviceID);

                        if (NetSession) {
                            OmniNet::OmniHeader Header;
                            Header.Target     = 0;
                            Header.PacketType = OmniNet::PacketType::ProcEdgeCross;
                            Header.Flags      = 0;

                            OmniEdgeCrossPacket EntryData = {};

                            EntryData.Edge = DeviceID;
                            EntryData.EdgeRelayMask =
                                !RemoteActive ? GetEdgeRelayMask(DeviceID) : 0;
                            EntryData.Y_Ratio = YRatio;
                            EntryData.X_Ratio = XRatio;

                            NetSession->SessionSend(
                                reinterpret_cast<CHAR*>(&EntryData),
                                sizeof(OmniEdgeCrossPacket),
                                Header
                            );

                            if (!RemoteActive) {
                                IOCtx.ActivateRemoteEdge(DeviceID);
                                SetCaptureHIDMode(ModeHID::CAPSEND);
                            } else {
                                SetCursorPos(
                                    Pos.x > 10 ? Pos.x - 5 : Pos.x + 5,
                                    Pos.y > 10 ? Pos.y - 5 : Pos.y + 5
                                );
                                CursorOwner.store(DeviceMap::C0, std::memory_order_release);
                                ResetEdgeRelayMask();
                            }

                        } else if (GetEdgeRelayState(DeviceID)) {

                            OmniNet::OmniHeader Header;
                            Header.Target     = 0;
                            Header.PacketType = OmniNet::PacketType::ProcEdgeCrossRelay;
                            Header.Flags      = 0;

                            OmniEdgeRelayPacket RelayData = {};

                            RelayData.RelayEdge = DeviceID;
                            RelayData.X_Ratio   = XRatio;
                            RelayData.Y_Ratio   = YRatio;

                            auto* NetSession = IOCtx.Router.GetSession(
                                CursorOwner.load(std::memory_order_acquire)
                            );

                            if (NetSession) {
                                NetSession->SessionSend(
                                    reinterpret_cast<CHAR*>(&RelayData),
                                    sizeof(OmniEdgeRelayPacket),
                                    Header
                                );
                            }

                            SetCursorPos(
                                Pos.x > 10 ? Pos.x - 5 : Pos.x + 5,
                                Pos.y > 10 ? Pos.y - 5 : Pos.y + 5
                            );
                            CursorOwner.store(DeviceMap::C0, std::memory_order_release);
                            ResetEdgeRelayMask();
                        }

                        break;
                    }
                }

                InputStateHID.store(false, std::memory_order_release);

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            if (!InputLinkStatus.load())
                break;

            MouseEventStatus->store(true);

            // Await until cursor returns home or breakout is triggered
            while (IOCtx.InputLocked.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            SetCaptureHIDMode(ModeHID::DEAD);

            if (!InputLinkStatus.load()) {
                break;
            }
        }
    });
}

void OmniInputLink::AddEdgeCondition(DeviceMap Index)
{
    const uint32_t W = IOCtx.Router.ResWidth;
    const uint32_t H = IOCtx.Router.ResHeight;

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

void OmniInputLink::RemoveEdgeCondition(DeviceMap DeviceID)
{
    ConditionManager.Remove(DeviceID);
}

void OmniInputLink::SetCaptureHIDMode(ModeHID TargetMode)
{
    ModeHID PrevMode = CaptureStateHID.exchange(TargetMode, std::memory_order_acq_rel);
    if (PrevMode == TargetMode)
        return;

    RAWINPUTDEVICE Devices[2]  = {};
    UINT           DeviceCount = 0;

    switch (TargetMode) {
    case ModeHID::HIDMON:
        // Monitor mouse and If coming directly from CAPSEND, unregister keyboard
        Devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Devices[0].usUsage     = HID_USAGE_GENERIC_MOUSE;
        Devices[0].dwFlags     = RIDEV_INPUTSINK;
        Devices[0].hwndTarget  = CaptureHWND;

        if (PrevMode == ModeHID::CAPSEND) {
            Devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
            Devices[1].usUsage     = HID_USAGE_GENERIC_KEYBOARD;
            Devices[1].dwFlags     = RIDEV_REMOVE;
            Devices[1].hwndTarget  = NULL;
            DeviceCount            = 2;
        } else {
            DeviceCount = 1;
        }

        InputProc = &OmniInputLink::InputProcCallbackHID;
        RegisterRawInputDevices(Devices, DeviceCount, sizeof(RAWINPUTDEVICE));
        break;

    case ModeHID::CAPSEND:
        // Capture both mouse and keyboard
        Devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Devices[0].usUsage     = HID_USAGE_GENERIC_MOUSE;
        Devices[0].dwFlags     = RIDEV_INPUTSINK;
        Devices[0].hwndTarget  = CaptureHWND;

        Devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Devices[1].usUsage     = HID_USAGE_GENERIC_KEYBOARD;
        Devices[1].dwFlags     = RIDEV_INPUTSINK;
        Devices[1].hwndTarget  = CaptureHWND;

        InputProc = &OmniInputLink::InputProcCallback;
        RegisterRawInputDevices(Devices, 2, sizeof(RAWINPUTDEVICE));
        break;

    case ModeHID::DEAD:
    default:
        InputProc = &OmniInputLink::VoidExitCallback;

        // Always unregister mouse
        Devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Devices[0].usUsage     = HID_USAGE_GENERIC_MOUSE;
        Devices[0].dwFlags     = RIDEV_REMOVE;
        Devices[0].hwndTarget  = NULL;

        // Unregister keyboard only if active before
        if (PrevMode == ModeHID::CAPSEND) {
            Devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
            Devices[1].usUsage     = HID_USAGE_GENERIC_KEYBOARD;
            Devices[1].dwFlags     = RIDEV_REMOVE;
            Devices[1].hwndTarget  = NULL;
            DeviceCount            = 2;
        } else {
            DeviceCount = 1;
        }

        RegisterRawInputDevices(Devices, DeviceCount, sizeof(RAWINPUTDEVICE));
        break;
    }
}

void OmniInputLink::InputProcInit(LPARAM& LParam)
{
    InputProcCallback(LParam);
}

void OmniInputLink::InputProcCallback(LPARAM& LParam)
{
    alignas(RAWINPUT) BYTE RawBuffer[sizeof(RAWINPUT)] = {};
    UINT                   Size                        = sizeof(RawBuffer);
    UINT                   Result =
        GetRawInputData((HRAWINPUT)LParam, RID_INPUT, RawBuffer, &Size, sizeof(RAWINPUTHEADER));
    if (Result == (UINT)-1 || Result == 0)
        return;

    RAWINPUT* Input = reinterpret_cast<RAWINPUT*>(RawBuffer);

    auto* NetSession =
        IOCtx.Router.GetSession(IOCtx.ActiveRemoteEdge.load(std::memory_order_acquire));
    if (!NetSession)
        return;

    if (Input->header.dwType == RIM_TYPEMOUSE) {
        LONG   dX          = Input->data.mouse.lLastX;
        LONG   dY          = Input->data.mouse.lLastY;
        USHORT ButtonFlags = Input->data.mouse.usButtonFlags;

        if ((dX | dY | ButtonFlags) == 0)
            return;

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

        if (ButtonFlags) [[unlikely]] {
            Packet.Buttons |= (ButtonFlags & 0x003F) << 1;

            if (ButtonFlags & (RI_MOUSE_WHEEL | RI_MOUSE_HWHEEL)) {
                Packet.Buttons |=
                    (ButtonFlags & RI_MOUSE_WHEEL) ? MOUSEEVENTF_WHEEL : MOUSEEVENTF_HWHEEL;
                Packet.Wheel = static_cast<SHORT>(Input->data.mouse.usButtonData);
            }
        }

        NetSession->SessionSend(reinterpret_cast<CHAR*>(&Packet), sizeof(OmniMousePacket), Header);
    }
}

void OmniInputLink::InputProcCallbackHID(LPARAM& LParam)
{
    InputStateHID.store(true, std::memory_order_release);
}

void OmniInputLink::VoidExitCallback(LPARAM& LParam)
{
    (void)LParam;
}

void OmniSynth::ProcMouse(int X, int Y)
{
    SetCursorPos(X, Y);
}

void OmniSynth::ProcEdgeCross(DeviceMap DeviceID, const OmniEdgeCrossPacket& Packet)
{
    DeviceMap ActiveLinkID = InputLink.CursorOwner.load(std::memory_order_acquire);
    if (ActiveLinkID != DeviceMap::C0) {
        auto* NetSession = IOContext.Router.GetSession(ActiveLinkID);
        if (NetSession) {
            OmniNet::OmniHeader Header;
            Header.Target     = 0;
            Header.PacketType = OmniNet::PacketType::ProcEdgeCross;
            Header.Flags      = 0;

            POINT CursorPos{};
            GetCursorPos(&CursorPos);

            uint16_t YRatio =
                (IOContext.Router.ResHeight > 0)
                    ? static_cast<uint16_t>(
                          (static_cast<uint64_t>(CursorPos.y) << 16) / IOContext.Router.ResHeight
                      )
                    : (1 << 15);
            uint16_t XRatio =
                (IOContext.Router.ResWidth > 0)
                    ? static_cast<uint16_t>(
                          (static_cast<uint64_t>(CursorPos.x) << 16) / IOContext.Router.ResWidth
                      )
                    : (1 << 15);

            OmniEdgeCrossPacket EntryData = {};

            EntryData.Edge    = ActiveLinkID;
            EntryData.X_Ratio = XRatio;
            EntryData.Y_Ratio = YRatio;

            NetSession->SessionSend(
                reinterpret_cast<CHAR*>(&EntryData), sizeof(OmniEdgeCrossPacket), Header
            );
        }
    }

    DeviceMap ActiveRemoteEdge = IOContext.ActiveRemoteEdge.load(std::memory_order_acquire);
    if (DeviceMap::C0 == ActiveRemoteEdge) { // Cursor home
        InputLink.SetEdgeRelayMask(DeviceID, Packet.EdgeRelayMask);
        InputLink.CursorOwner.store(DeviceID, std::memory_order_release);
    } else if (DeviceID == ActiveRemoteEdge) { // Cursor returned..
        InputLink.CursorOwner.store(DeviceMap::C0, std::memory_order_release);

        InputLink.SetCaptureHIDMode(ModeHID::DEAD);
        IOContext.DeactivateRemoteEdge();

    } else { // Cursor went out but new guests on the door
        auto* NetSession = IOContext.Router.GetSession(ActiveRemoteEdge);
        if (NetSession) {
            OmniNet::OmniHeader Header{OmniNet::PacketType::ProcEdgeRecall, 0, 0};
            NetSession->SessionSend(0, 0, Header);
        }

        InputLink.SetEdgeRelayMask(DeviceID, Packet.EdgeRelayMask);
        InputLink.CursorOwner.store(DeviceID, std::memory_order_release);

        InputLink.SetCaptureHIDMode(ModeHID::DEAD);
        IOContext.DeactivateRemoteEdge();
    }

    Device::MonitorRes Res = Device::GetMonitorResolution();

    int TargetY =
        (Res.Height > 0)
            ? static_cast<int>((static_cast<uint64_t>(Packet.Y_Ratio) * (Res.Height - 1)) >> 16)
            : static_cast<int>(Res.Height >> 1);

    int TargetX =
        (Res.Width > 0)
            ? static_cast<int>((static_cast<uint64_t>(Packet.X_Ratio) * (Res.Width - 1)) >> 16)
            : static_cast<int>(Res.Width >> 1);

    DeviceMap Edge = Packet.Edge;
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
}

void OmniSynth::ProcEdgeRelayCross(DeviceMap DeviceID, const OmniEdgeRelayPacket& Packet)
{

    DeviceMap ReverseID = ComputeRelativeSpartialID(DeviceID, DeviceMap::C0);
    DeviceMap TargetID  = ComputeRelativeSpartialID(ReverseID, Packet.RelayEdge);

    auto* NetSession = IOContext.Router.GetSession(TargetID);
    if (NetSession) {

        OmniNet::OmniHeader Header;
        Header.Target     = 0;
        Header.PacketType = OmniNet::PacketType::ProcEdgeCross;
        Header.Flags      = 0;

        OmniEdgeCrossPacket EntryData = {};

        EntryData.Edge          = Packet.RelayEdge;
        EntryData.EdgeRelayMask = InputLink.GetEdgeRelayMask(TargetID);
        EntryData.X_Ratio       = Packet.X_Ratio;
        EntryData.Y_Ratio       = Packet.Y_Ratio;

        NetSession->SessionSend(
            reinterpret_cast<CHAR*>(&EntryData), sizeof(OmniEdgeCrossPacket), Header
        );

        IOContext.ActivateRemoteEdge(TargetID);
    } else {
        IOContext.DeactivateRemoteEdge();
        InputLink.SetCaptureHIDMode(ModeHID::DEAD);
    }
}

void OmniSynth::ProcEdgeRecall(DeviceMap DeviceID)
{
    InputLink.CursorOwner.store(DeviceMap::C0, std::memory_order_release);
    InputLink.ResetEdgeRelayMask();
}

void OmniSynth::ProcMouse(const OmniMousePacket& Packet)
{
    if (Packet.Flags & OMNI_MOUSE_ABSOLUTE) {
        INPUT MouseInput        = {0};
        MouseInput.type         = INPUT_MOUSE;
        MouseInput.mi.dx        = Packet.dX;
        MouseInput.mi.dy        = Packet.dY;
        MouseInput.mi.mouseData = Packet.Wheel;
        MouseInput.mi.dwFlags =
            MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | Packet.Buttons;
        MouseInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
        SendInput(1, &MouseInput, sizeof(INPUT));
        return;
    }

    if (GameMode.load(std::memory_order_relaxed)) {
        INPUT MouseInput          = {0};
        MouseInput.type           = INPUT_MOUSE;
        MouseInput.mi.dx          = Packet.dX;
        MouseInput.mi.dy          = Packet.dY;
        MouseInput.mi.mouseData   = Packet.Wheel;
        MouseInput.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_MOVE_NOCOALESCE | Packet.Buttons;
        MouseInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
        SendInput(1, &MouseInput, sizeof(INPUT));
        return;
    }

    if (Packet.dX | Packet.dY) {
        POINT pt = {};
        GetCursorPos(&pt);
        SetCursorPos(pt.x + Packet.dX, pt.y + Packet.dY);
    }

    if (Packet.Buttons | Packet.Wheel) {
        INPUT BtnInput          = {0};
        BtnInput.type           = INPUT_MOUSE;
        BtnInput.mi.mouseData   = Packet.Wheel;
        BtnInput.mi.dwFlags     = Packet.Buttons;
        BtnInput.mi.dwExtraInfo = OMNI_INPUT_COOKIE;
        SendInput(1, &BtnInput, sizeof(INPUT));
    }
}

void OmniSynth::ProcKey(const OmniKeyPacket& Packet)
{
    INPUT KB          = {};
    KB.type           = INPUT_KEYBOARD;
    KB.ki.wVk         = Packet.VkCode;
    KB.ki.wScan       = Packet.ScanCode;
    KB.ki.dwFlags     = Packet.Flags;
    KB.ki.dwExtraInfo = OMNI_INPUT_COOKIE;

    SendInput(1, &KB, sizeof(INPUT));
}

void OmniSynth::ProcInput(INPUT& Input)
{
    if (Input.type == INPUT_MOUSE) {
        if (Input.mi.dwFlags & MOUSEEVENTF_ABSOLUTE) {
            SendInput(1, &Input, sizeof(INPUT));
        } else {
            if (Input.mi.dx | Input.mi.dy) {
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

void OmniSynth::ProcKey(INPUT& Input)
{
    SendInput(1, &Input, sizeof(INPUT));
}

void OmniSynth::ProcKey(KeyData& Input)
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
