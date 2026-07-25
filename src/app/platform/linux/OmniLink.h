#ifndef OMNILINK_H
#define OMNILINK_H

#pragma once
#include "resource.h"

#include "OmniCore.h"
#include "OmniGUI.h"
#include "SystemLink.h"

#include <mutex>
#include <unordered_map>

class OmniLink : public OmniCore
{
  private:
    OmniGUI* GUI = nullptr;

    std::chrono::steady_clock::duration FrameTimeLimit = std::chrono::nanoseconds(15 * 1000000);

    std::chrono::time_point<std::chrono::steady_clock> LastFrameTime =
        std::chrono::steady_clock::now();

    std::mutex EventTokensMutex;
    std::unordered_map<DeviceMap, std::shared_ptr<std::atomic<bool>>> ActiveEventTokens;

    void OmniMainLoop();

    void InitTrayIcon();

  public:
    OmniLink();

    void OmniMain();

    void PushNotification(const Notification& notification) override
    {
        if (GUI)
            GUI->PushNotification(notification);
    }

    void PushNotification(DeviceMap DeviceID, const Notification& notification) override
    {
        if (DeviceID != DeviceMap::END && notification.Cancelled) {
            std::lock_guard<std::mutex> lock(EventTokensMutex);
            ActiveEventTokens[DeviceID] = notification.Cancelled;
        }

        if (GUI)
            GUI->PushNotification(notification);
    }

    void CancelNotification(DeviceMap DeviceID) override
    {
        std::lock_guard<std::mutex> lock(EventTokensMutex);
        auto iter = ActiveEventTokens.find(DeviceID);
        if (iter != ActiveEventTokens.end()) {
            if (iter->second) {
                iter->second->store(true, std::memory_order_relaxed);
            }
            ActiveEventTokens.erase(iter);
        }
    }
};

#endif
