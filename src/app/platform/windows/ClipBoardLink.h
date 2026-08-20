#pragma once

#include <Windows.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

class ClipBoardLink
{
  public:
    using ClipboardChangeCallback = std::function<void(const std::string&)>;

    ClipBoardLink() = default;
    ~ClipBoardLink();

    void StartMonitoring(HWND WindowHandle, ClipboardChangeCallback Callback);
    void StopMonitoring();

    void OnClipboardUpdate();

    static bool SetClipTypeText(const std::string& Text);
    static std::string GetClipTypeText();

    bool GetState() const { return HookState.load(); }

  private:
    HWND WindowID = nullptr;
    std::atomic<bool> HookState{false};
    ClipboardChangeCallback ChangeCallback;

    static inline DWORD LastSequenceNumber = 0;
    static inline std::string LastText = "";
    static inline std::mutex TextMutex;
};
