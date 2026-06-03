#ifndef IOLINK_H
#define IOLINK_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#pragma once
#include "Helper.h"
#include "OmniInstances.h"

#include <Windows.h>
#include <array>
#include <atomic>
#include <hidusage.h>
#include <mutex>
#include <windows.h>

struct MouseXY
{
    int32_t X;
    int32_t Y;

    MouseXY(int x, int y)
    {
        X = x;
        Y = y;
    }
};

struct KeyData
{
    USHORT MakeCode;
    USHORT Flags;
};

struct Point
{
    LONG x;
    LONG y;
};

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

class session;

extern std::atomic<bool> LockState;

class OmniShield
{
  public:
    OmniShield();

    void InvokeInputFilter();

    void ReleaseInputFilter();

    static LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    static LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);

  private:
    HHOOK KeyboardBlock = NULL;
    HHOOK MouseBlock = NULL;
};

class OmniCap
{
  public:
    OmniCap(ActiveInstanceContainer& ctx);

    // Mouse cursor position used by both edge detection and high performance
    // input capture
    int MouseX = 0;
    int MouseY = 0;

    /// ##########################################################################################
    /// ///
    ///	Display Edge Detection For the Mouse
    //////
    /// ##########################################################################################
    /// ///

    unsigned int ResWidth = 0;
    unsigned int ResHeight = 0;

    FlowMorph<int, int, DeviceMap> ConditionManager;

    void ToggleEdgeProbe(HWND hwnd);

    bool GetEdgeProbeState();

    void CreateEdgeProbe(HWND hwnd, bool state = true);

    void AddEdgeCondition(DeviceMap Index);

    /// ##########################################################################################
    /// /// High Perofrmance Input Capture
    /// ///
    /// ##########################################################################################
    /// ///

    void (OmniCap::*InputProc)(LPARAM& lParam) = nullptr;
    void ToggleInputCapture(HWND hwnd, bool state = false);

    // Initial mouse input event proc used for calculating the size of the raw
    // input struct
    void InputProcInit(LPARAM& lParam);

    // Default mouse input event proc for high performance input capturing
    void InputProcCallback(LPARAM& lParam);

    // Termination sequence for input capturing process
    void VoidExitCallback(LPARAM& lParam);

    // for future usage if dynamic assignment of input capture handling is needed
    /*void (*OnMouseCapture)(RAWINPUT& Input) = nullptr;

    void (*OnKeyboardCapture)(RAWINPUT& RawInput) = nullptr;

    void (*OnInitialMouseCapture)(int MouseX, int MouseY) = nullptr;*/

    /// ##########################################################################################
    /// /// Window Move Event Detection
    /// ///
    /// ##########################################################################################
    /// ///

    void WindowMoveListener(bool state = false);

    inline void SetActiveSession(session* target) { ActiveSession = target; }

  private:
    std::atomic_bool InputLinkStatus = false;

    DeviceMap ActiveEdgeCondition;
    session* ActiveSession = nullptr;
    ActiveInstanceContainer& ActiveSessions;

    std::unordered_map<DeviceMap, std::function<bool(int, int)>>& Conditions =
        ConditionManager.conditions;

    std::mutex ConditionMutex;

    std::atomic_bool MouseEventCapStatus;
    HWINEVENTHOOK WinCapHook = NULL;
    UINT RawInputSize;

    // Callback for window movement detection
    static void CALLBACK WinMvEventProc(HWINEVENTHOOK hWinEventHook,
                                        DWORD event,
                                        HWND hwnd,
                                        LONG idObject,
                                        LONG idChild,
                                        DWORD idEventThread,
                                        DWORD dwmsEventTime);
};

class OmniSynth
{
  public:
    int MouseX = 0;
    int MouseY = 0;

    // Sets current cursor position using absolute pixel cordinates
    void static ProcMouse(int x, int y);

    void static ProcInput(INPUT& input);

    // Simulate keyboard button actions
    void static ProcKey(INPUT& input);

    // Simulate keyboard button actions
    void static ProcKey(KeyData& input);

    void SetMouseCursor(int MouseX, int MouseY);

    // Move cursor by pixel count rather than set cursor to an exact position
    // Set current cursor position before using this function in order to avoid
    // incorrect starting points
    void inline MvMouse(int toX, int toY)
    {
        MouseX += toX;
        MouseY += toY;

        SetCursorPos(MouseX, MouseY);
    }

    // Returns true if the current registered mouse position matches with the give
    // positions
    bool inline CheckMousePos(int MX, int MY)
    {
        if (MX != MouseX && MY != MouseY) {
            return false;
        } else
            return true;
    }

    // MouseXY inline GetCursorPos() {}
};

#endif
