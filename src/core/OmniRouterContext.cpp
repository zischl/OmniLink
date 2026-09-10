#include "OmniRouterContext.h"

OmniRouterContext::OmniRouterContext()
{
    for (auto& Slot : Sessions) {
        Slot.store(nullptr, std::memory_order_relaxed);
    }
}

void OmniRouterContext::SetResolution(uint32_t Width, uint32_t Height)
{
    ResWidth.store(Width, std::memory_order_release);
    ResHeight.store(Height, std::memory_order_release);
}

void OmniRouterContext::RegisterSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session)
{
    if (DeviceID < DeviceMap::END) {
        Sessions[DeviceID].store(Session, std::memory_order_release);
    }
}

void OmniRouterContext::UnregisterSession(DeviceMap DeviceID)
{
    if (DeviceID < DeviceMap::END) {
        Sessions[DeviceID].store(nullptr, std::memory_order_release);
    }
}

bool OmniRouterContext::GetSessionState(DeviceMap Edge) const
{
    if (Edge < DeviceMap::END) {
        return Sessions[Edge].load(std::memory_order_acquire) != nullptr;
    }
    return false;
}

OmniNetSession<OmniMTU>* OmniRouterContext::GetSession(DeviceMap Edge) const
{
    if (Edge < DeviceMap::END) {
        return Sessions[Edge].load(std::memory_order_acquire);
    }
    return nullptr;
}

void OmniRouterContext::Reset()
{
    for (auto& Slot : Sessions) {
        Slot.store(nullptr, std::memory_order_release);
    }
}
