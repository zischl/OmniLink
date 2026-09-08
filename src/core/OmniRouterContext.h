#pragma once

#include "OmniConfig.h"
#include "OmniEnums.h"

#include <array>
#include <atomic>
#include <cstdint>

template <uint32_t MTU> class OmniNetSession;

// 3x3 device matrix router and display context.
// Links together Net Sessions and screen edge routing.
struct OmniRouterContext
{
    std::atomic<uint32_t> ResWidth{0};
    std::atomic<uint32_t> ResHeight{0};

    OmniRouterContext();
    ~OmniRouterContext() = default;

    OmniRouterContext(const OmniRouterContext&)            = delete;
    OmniRouterContext& operator=(const OmniRouterContext&) = delete;

    void SetResolution(uint32_t Width, uint32_t Height);

    void RegisterSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session);
    void UnregisterSession(DeviceMap DeviceID);

    bool GetSessionState(DeviceMap Edge) const;

    OmniNetSession<OmniMTU>* GetSession(DeviceMap Edge) const;

    void Reset();

  private:
    std::array<std::atomic<OmniNetSession<OmniMTU>*>, DeviceMap::END> Sessions;
};
