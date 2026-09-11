#include "OmniRouterContext.h"

OmniRouterContext::OmniRouterContext()
{
    for (size_t i = 0; i < DeviceMap::END; ++i) {
        Sessions[i].store(nullptr, std::memory_order_relaxed);
        WindowSessions[i].store(nullptr, std::memory_order_relaxed);
        EdgeResWidth[i].store(0, std::memory_order_relaxed);
        EdgeResHeight[i].store(0, std::memory_order_relaxed);
    }
}

void OmniRouterContext::SetResolution(uint32_t Width, uint32_t Height)
{
    ResWidth.store(Width, std::memory_order_release);
    ResHeight.store(Height, std::memory_order_release);
}

void OmniRouterContext::SetDeviceResolution(DeviceMap Edge, uint32_t Width, uint32_t Height)
{
    if (Edge < DeviceMap::END) {
        EdgeResWidth[Edge].store(Width, std::memory_order_release);
        EdgeResHeight[Edge].store(Height, std::memory_order_release);
    }
}

void OmniRouterContext::GetDeviceResolution(DeviceMap Edge, uint32_t& Width, uint32_t& Height) const
{
    Width  = 0;
    Height = 0;
    if (Edge < DeviceMap::END) {
        Width  = EdgeResWidth[Edge].load(std::memory_order_acquire);
        Height = EdgeResHeight[Edge].load(std::memory_order_acquire);
    }
    if (Width == 0) {
        Width = ResWidth.load(std::memory_order_relaxed);
    }
    if (Height == 0) {
        Height = ResHeight.load(std::memory_order_relaxed);
    }
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
        WindowSessions[DeviceID].store(nullptr, std::memory_order_release);
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

void OmniRouterContext::RegisterWindowSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session)
{
    if (DeviceID < DeviceMap::END) {
        WindowSessions[DeviceID].store(Session, std::memory_order_release);
    }
}

void OmniRouterContext::UnregisterWindowSession(DeviceMap DeviceID)
{
    if (DeviceID < DeviceMap::END) {
        WindowSessions[DeviceID].store(nullptr, std::memory_order_release);
    }
}

OmniNetSession<OmniMTU>* OmniRouterContext::GetWindowSession(DeviceMap Edge) const
{
    if (Edge < DeviceMap::END) {
        return WindowSessions[Edge].load(std::memory_order_acquire);
    }
    return nullptr;
}

void OmniRouterContext::Reset()
{
    for (size_t i = 0; i < DeviceMap::END; ++i) {
        Sessions[i].store(nullptr, std::memory_order_release);
        WindowSessions[i].store(nullptr, std::memory_order_release);
        EdgeResWidth[i].store(0, std::memory_order_release);
        EdgeResHeight[i].store(0, std::memory_order_release);
    }
}
