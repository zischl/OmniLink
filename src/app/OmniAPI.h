#ifndef OMNIAPI_H
#define OMNIAPI_H

#pragma once
#include "OmniPackets.h"
#include <chrono>
#include <iostream>

class OmniLink;

class OmniAPI
{
  public:
    static void Ignite(OmniLink& OmniLinkInstance);

    static void SwapDeviceLayout(uint8_t index1, uint8_t index2);

    static void Scan();

    static void Connect(ConnectionRequest Rquest);

    static void AcceptHandshake(DeviceMap DeviceID, bool TrustPermanently = false);

    static void RejectHandshake(DeviceMap DeviceID);

    static void CancelHandshake(DeviceMap DeviceID);

    static void ExecuteNetCommand(CoreCommands Command);

    static void ExecuteNetCommandWArgs(OmniCommand Command);

    static bool VerifyCommandToken(DeviceMap DeviceID, const OmniNetCommand& Command);

    static void ToggleFeature(FeatureTypes FeatureIndex, DeviceMap Index);

    static void Get(DataTypes);

    inline static void perf_test_start() { t1 = std::chrono::high_resolution_clock::now(); }

    inline static void perf_test_end()
    {
        std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::high_resolution_clock::now() - t1
                     )
                         .count()
                  << "Time Taken : : : : :\n";
    }

  private:
    static inline OmniLink* App = nullptr;

    static inline std::chrono::time_point<std::chrono::high_resolution_clock> t1;
};

#endif
