#pragma once

#include "ClipboardTypes.h"

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
    using ClipboardManifestCallback = std::function<void(const ClipboardManifest&)>;

    ClipBoardLink() = default;
    ~ClipBoardLink();

    void StartMonitoring(
        HWND WindowHandle,
        ClipboardChangeCallback LightGramCB,
        ClipboardManifestCallback ManifestCB = nullptr
    );
    void StopMonitoring();

    void OnClipboardUpdate();
    void OnPasteRequest(UINT uFormat);
    void OnRequestInvalidation();

    static bool SetClipTypeText(const std::string& Text);
    static std::string GetClipTypeText();

    static void AddClipItemPromise(const ClipboardManifest& Manifest);

    bool GetState() const { return HookState.load(); }

  private:
    static inline HWND WindowID = nullptr;
    std::atomic<bool> HookState{false};
    ClipboardChangeCallback ChangeCallback;
    ClipboardManifestCallback ManifestCallback;

    static inline DWORD LastSequenceNumber = 0;
    static inline std::string LastText = "";
    static inline std::mutex TextMutex;

    static inline ClipboardManifest PendingManifest;
    static inline std::mutex ManifestMutex;
};
