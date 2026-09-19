#pragma once

#include "OmniConfig.hpp"
#include "OmniEnums.hpp"

#include <array>
#include <atomic>
#include <cstdint>

template <uint32_t MTU> class OmniNetSession;

// 3x3 device matrix router and display context.
// Links together Net Sessions and screen edge routing.
struct OmniRouter
{
    std::atomic<uint32_t> ResWidth{0};
    std::atomic<uint32_t> ResHeight{0};

    OmniRouter();
    ~OmniRouter() = default;

    OmniRouter(const OmniRouter&)            = delete;
    OmniRouter& operator=(const OmniRouter&) = delete;

    void SetResolution(uint32_t Width, uint32_t Height);

    void SetDeviceResolution(DeviceMap Edge, uint32_t Width, uint32_t Height);
    void GetDeviceResolution(DeviceMap Edge, uint32_t& Width, uint32_t& Height) const;

    void RegisterSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session);
    void UnregisterSession(DeviceMap DeviceID);

    bool GetSessionState(DeviceMap Edge) const;

    OmniNetSession<OmniMTU>* GetSession(DeviceMap Edge) const;

    void RegisterWindowSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session);
    void UnregisterWindowSession(DeviceMap DeviceID);
    OmniNetSession<OmniMTU>* GetWindowSession(DeviceMap Edge) const;

    inline uint32_t GetWindowSessionCount() const
    {
        return WindowSessionCount.load(std::memory_order_acquire);
    }

    void Reset();

  private:
    std::array<std::atomic<OmniNetSession<OmniMTU>*>, DeviceMap::END> Sessions;
    std::array<std::atomic<OmniNetSession<OmniMTU>*>, DeviceMap::END> WindowSessions;
    std::atomic<uint32_t>                                             WindowSessionCount{0};
    std::array<std::atomic<uint32_t>, DeviceMap::END>                 EdgeResWidth;
    std::array<std::atomic<uint32_t>, DeviceMap::END>                 EdgeResHeight;
};
