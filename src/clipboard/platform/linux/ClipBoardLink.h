#pragma once

#include "ClipboardTypes.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class ClipBoardLink
{
  public:
    using ClipboardChangeCallback = std::function<void(const std::string&)>;
    using ClipboardManifestCallback = std::function<void(const ClipboardManifest&)>;
    using PasteRequestCallback =
        std::function<std::vector<uint8_t>(const ClipboardManifest&, uint32_t)>;

    ClipBoardLink() = default;
    ~ClipBoardLink();

    void StartMonitoring(
        ClipboardChangeCallback LightGramCB, ClipboardManifestCallback ManifestCB = nullptr
    );
    void StopMonitoring();

    static bool SetClipTypeText(const std::string& Text);
    static std::string GetClipTypeText();

    static void AddClipItemPromise(const ClipboardManifest& Manifest);
    static void SetPasteRequestCallback(PasteRequestCallback Callback);

    bool GetState() const { return Monitoring.load(); }

  private:
    void MonitorLoop();

    std::atomic<bool> Monitoring{false};
    std::thread MonitorThread;
    ClipboardChangeCallback ChangeCallback;
    ClipboardManifestCallback ManifestCallback;

    static inline uint64_t LastSequenceNumber = 0;
    static inline std::string LastText = "";
    static inline std::mutex TextMutex;

    static inline ClipboardManifest PendingManifest;
    static inline std::mutex ManifestMutex;
    static inline PasteRequestCallback PRequestCallback = nullptr;
};
