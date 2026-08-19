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

    bool SetClipTypeText(const std::string& Text);
    std::string GetClipTypeText();

    bool GetState() const { return HookState.load(); }

  private:
    HWND WindowID = nullptr;
    std::atomic<bool> HookState{false};
    ClipboardChangeCallback ChangeCallback;

    DWORD LastSequenceNumber = 0;
    std::string LastText;
    std::mutex TextMutex;
};
