#pragma once

#include "Helper.hpp"
#include "IOLinkContext.hpp"

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

template <uint32_t MTU> class OmniNetSession;

// Installs keyboard/mouse hooks that suppress local input base on InputLocked
class OmniInputFilter
{
  public:
    explicit OmniInputFilter(InputLinkContext& Ctx);

    void InvokeInputFilter();
    void ReleaseInputFilter();

    static InputLinkContext* GetContext() { return IOContext; }

  private:
    InputLinkContext& IOCtx;

    HHOOK KeyboardBlock = NULL;
    HHOOK MouseBlock    = NULL;

    static LRESULT CALLBACK KeyboardProc(int NCode, WPARAM WParam, LPARAM LParam);
    static LRESULT CALLBACK MouseProc(int NCode, WPARAM WParam, LPARAM LParam);

    static InputLinkContext* IOContext;
};

// Handles screen-edge detection and high-performance raw input capture.
class OmniInputLink
{
  public:
    explicit OmniInputLink(InputLinkContext& Ctx);
    ~OmniInputLink();

    // Mouse cursor position tracked locally for edge detection and delta math.
    int MouseX = 0;
    int MouseY = 0;

    FlowMorph<int, int, DeviceMap> ConditionManager;

    // The Edge Probe system worls alongside the FlowMorph dynamic ConditionManager.
    // Based on registered directions and the user's display resolution monitors cursor edge hits.
    // Calculates scaled X and Y ratio from user resolution and Cursor X Y pos..
    // Transmits OmniEdgeCrossPacket to transfer cursor ownership and awaits return
    void ToggleEdgeProbe();
    bool GetEdgeProbeState();
    void AddEdgeCondition(DeviceMap Index);

    void (OmniInputLink::*InputProc)(LPARAM& LParam) = nullptr;

    // High Performance Input Capture
    void ToggleInputCapture(bool State);

    // Just calls the actual InputProcCallback, will later update to handle WM_INPUT_DEVICE_CHANGE
    void InputProcInit(LPARAM& LParam);

    // Called for every raw mouse/keyboard event while captured.
    void InputProcCallback(LPARAM& LParam);

    // Drain callback used during the teardown window.
    void VoidExitCallback(LPARAM& LParam);

    // Event-Driven Focus Detection, Mainly for games
    void FocusEventListener(bool State = true);

  private:
    InputLinkContext& IOCtx;
    HWND              CaptureHWND;

    std::atomic_bool InputLinkStatus{false};
    std::atomic_bool MouseEventCapStatus{false};

    DeviceMap ActiveEdgeCondition{DeviceMap::C0};

    std::unordered_map<DeviceMap, std::function<bool(int, int)>>& Conditions =
        ConditionManager.conditions;

    std::mutex ConditionMutex;

    HWINEVENTHOOK WinFocusHook = NULL;
    UINT          RawInputSize;

    std::thread ProbeThread;

    void CreateEdgeProbe();
    void StopEdgeProbe();

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
// SendInput / SetCursorPos calls.
class OmniSynth
{
  private:
    OmniInputLink& InputLink;

  public:
    static std::atomic<bool> GameMode;

    explicit OmniSynth(OmniInputLink& InputLink) : InputLink(InputLink) {}

    // Process a OmniMousePacket for hybrid SetCursorPos + SendInput behaviour
    void ProcMouse(const OmniMousePacket& Packet);

    // Process an incoming OmniEdgeCrossPacket for proportional entry and.. return
    void ProcEdgeCross(const OmniEdgeCrossPacket& Packet);

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
};
