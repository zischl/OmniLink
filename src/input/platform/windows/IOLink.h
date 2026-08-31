#pragma once

#include "Helper.h"
#include "IOLinkContext.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <hidusage.h>
#include <mutex>
#include <thread>

struct MouseXY
{
    int32_t X;
    int32_t Y;

    MouseXY(int X, int Y) : X(X), Y(Y) {}
};

struct KeyData
{
    USHORT MakeCode;
    USHORT Flags;
};

struct Point
{
    LONG X;
    LONG Y;
};

// Absolute cursor positions for each screen edge
static constexpr std::array<Point, 9> PointCache = {{
    {32767, 32767}, // C0
    {65535, 32767}, // L1
    {32767, 65535}, // U1
    {0, 32767},     // R1
    {32767, 0},     // D1
    {65535, 65535}, // LU1
    {0, 65535},     // RU1
    {0, 0},         // RD1
    {65535, 0}      // LD1
}};

template <uint32_t MTU> class OmniNetSession;

// Installs keyboard/mouse hooks that suppress local input base on InputLocked
class OmniIOShield
{
  public:
    explicit OmniIOShield(IOLinkContext& Ctx);

    void InvokeInputFilter();
    void ReleaseInputFilter();

    static IOLinkContext* GetContext() { return IOContext; }

  private:
    IOLinkContext& IOCtx;

    HHOOK KeyboardBlock = NULL;
    HHOOK MouseBlock    = NULL;

    static LRESULT CALLBACK KeyboardProc(int NCode, WPARAM WParam, LPARAM LParam);
    static LRESULT CALLBACK MouseProc(int NCode, WPARAM WParam, LPARAM LParam);

    static IOLinkContext* IOContext;
};

// Handles screen-edge detection and high-performance raw input capture.
class OmniIOCap
{
  public:
    explicit OmniIOCap(IOLinkContext& Ctx);
    ~OmniIOCap();

    // Mouse cursor position tracked locally for edge detection and delta math.
    int MouseX      = 0;
    int MouseY      = 0;
    int VirtualPosX = 0;
    int VirtualPosY = 0;

    FlowMorph<int, int, DeviceMap> ConditionManager;

    void ToggleEdgeProbe(HWND Hwnd);
    bool GetEdgeProbeState();
    void AddEdgeCondition(DeviceMap Index);

    // High Performance Input Capture

    void (OmniIOCap::*InputProc)(LPARAM& LParam) = nullptr;

    void ToggleInputCapture(HWND Hwnd, bool State);

    // Queries raw input struct size, sends initial warp packet.
    void InputProcInit(LPARAM& LParam);

    // Called for every raw mouse/keyboard event while captured.
    void InputProcCallback(LPARAM& LParam);

    // Drain callback used during the teardown window.
    void VoidExitCallback(LPARAM& LParam);

    // Window Move Detection
    void WindowMoveListener(bool State = false);

    // Event-Driven Focus Detection, Mainly for games
    void FocusEventListener(bool State = true);

  private:
    IOLinkContext& IOCtx;

    std::atomic_bool InputLinkStatus{false};
    std::atomic_bool MouseEventCapStatus{false};

    DeviceMap ActiveEdgeCondition{DeviceMap::C0};

    std::unordered_map<DeviceMap, std::function<bool(int, int)>>& Conditions =
        ConditionManager.conditions;

    std::mutex ConditionMutex;

    HWINEVENTHOOK WinCapHook   = NULL;
    HWINEVENTHOOK WinFocusHook = NULL;
    UINT          RawInputSize;

    std::thread ProbeThread;

    void CreateEdgeProbe(HWND Hwnd);
    void StopEdgeProbe();

    static void CALLBACK WinMvEventProc(
        HWINEVENTHOOK HWinEventHook,
        DWORD         Event,
        HWND          Hwnd,
        LONG          IDObject,
        LONG          IDChild,
        DWORD         IDEventThread,
        DWORD         DWMSEventTime
    );

    static void CALLBACK WinFocusEventProc(
        HWINEVENTHOOK HWinEventHook,
        DWORD         Event,
        HWND          Hwnd,
        LONG          IDObject,
        LONG          IDChild,
        DWORD         IDEventThread,
        DWORD         DWMSEventTime
    );
};

// Pure input synthesis that translates received network packets into local
// SendInput / SetCursorPos calls. Fully stateless btw.
namespace OmniSynth {
extern std::atomic<bool> GameMode;

// Process a OmniMousePacket for hybrid SetCursorPos + SendInput behaviour
void ProcMouse(const OmniMousePacket& Packet);

// Process an incoming OmniBoundaryPacket for proportional entry and.. return
void ProcBoundary(const OmniBoundaryPacket& Packet);

// Process an incoming OmniKeyPacket for.. keys.. obviously..
void ProcKey(const OmniKeyPacket& Packet);

// Move cursor to absolute pixel position.
void ProcMouse(int X, int Y);

// Dispatch a INPUT struct either mouse or keyboard.
void ProcInput(INPUT& Input);

// Simulate a keyboard event from a INPUT struct.
void ProcKey(INPUT& Input);

// Simulate a keyboard event from a raw KeyData.
void ProcKey(KeyData& Input);

// Move cursor by a pixel delta relative to a known base position.
inline void MvMouse(int& CurrentX, int& CurrentY, int DX, int DY)
{
    CurrentX += DX;
    CurrentY += DY;
    SetCursorPos(CurrentX, CurrentY);
}

// Returns true only when both coordinates match.
inline bool CheckMousePos(int TrackedX, int TrackedY, int MX, int MY)
{
    return MX == TrackedX && MY == TrackedY;
}
} // namespace OmniSynth
