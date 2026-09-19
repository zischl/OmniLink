#include "OmniRouterContext.hpp"
#include <atomic>

OmniRouter::OmniRouter()
{
    for (size_t i = 0; i < DeviceMap::END; ++i) {
        Sessions[i].store(nullptr, std::memory_order_relaxed);
        WindowSessions[i].store(nullptr, std::memory_order_relaxed);
        EdgeResWidth[i].store(0, std::memory_order_relaxed);
        EdgeResHeight[i].store(0, std::memory_order_relaxed);
    }
}

void OmniRouter::SetResolution(uint32_t Width, uint32_t Height)
{
    ResWidth.store(Width, std::memory_order_release);
    ResHeight.store(Height, std::memory_order_release);
}

void OmniRouter::SetDeviceResolution(DeviceMap Edge, uint32_t Width, uint32_t Height)
{
    if (Edge < DeviceMap::END) {
        EdgeResWidth[Edge].store(Width, std::memory_order_release);
        EdgeResHeight[Edge].store(Height, std::memory_order_release);
    }
}

void OmniRouter::GetDeviceResolution(DeviceMap Edge, uint32_t& Width, uint32_t& Height) const
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

void OmniRouter::RegisterSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session)
{
    if (DeviceID < DeviceMap::END) {
        Sessions[DeviceID].store(Session, std::memory_order_release);
    }
}

void OmniRouter::UnregisterSession(DeviceMap DeviceID)
{
    if (DeviceID < DeviceMap::END) {
        Sessions[DeviceID].store(nullptr, std::memory_order_release);
        auto* PrevSession = WindowSessions[DeviceID].exchange(nullptr, std::memory_order_release);
        if (PrevSession)
            WindowSessionCount.fetch_sub(1, std::memory_order_acq_rel);
    }
}

bool OmniRouter::GetSessionState(DeviceMap Edge) const
{
    if (Edge < DeviceMap::END) {
        return Sessions[Edge].load(std::memory_order_acquire) != nullptr;
    }
    return false;
}

OmniNetSession<OmniMTU>* OmniRouter::GetSession(DeviceMap Edge) const
{
    if (Edge < DeviceMap::END) {
        return Sessions[Edge].load(std::memory_order_acquire);
    }
    return nullptr;
}

void OmniRouter::RegisterWindowSession(DeviceMap DeviceID, OmniNetSession<OmniMTU>* Session)
{
    if (DeviceID < DeviceMap::END) {
        auto* PrevSession = WindowSessions[DeviceID].exchange(Session, std::memory_order_release);

        if (!PrevSession && Session)
            WindowSessionCount.fetch_add(1, std::memory_order_acq_rel);
    }
}

void OmniRouter::UnregisterWindowSession(DeviceMap DeviceID)
{
    if (DeviceID < DeviceMap::END) {
        auto* PrevSession = WindowSessions[DeviceID].exchange(nullptr, std::memory_order_release);
        if (PrevSession)
            WindowSessionCount.fetch_sub(1, std::memory_order_acq_rel);
    }
}

OmniNetSession<OmniMTU>* OmniRouter::GetWindowSession(DeviceMap Edge) const
{
    if (Edge < DeviceMap::END) {
        return WindowSessions[Edge].load(std::memory_order_acquire);
    }
    return nullptr;
}

void OmniRouter::Reset()
{
    WindowSessionCount.store(0, std::memory_order_release);
    for (size_t i = 0; i < DeviceMap::END; ++i) {
        Sessions[i].store(nullptr, std::memory_order_release);
        WindowSessions[i].store(nullptr, std::memory_order_release);
        EdgeResWidth[i].store(0, std::memory_order_release);
        EdgeResHeight[i].store(0, std::memory_order_release);
    }
}
