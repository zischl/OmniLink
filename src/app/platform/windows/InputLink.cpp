#include "InputLink.h"
#include "OmniInstances.h"
#include "OmniLogger.hpp"
#include "OmniRouterContext.hpp"
#include "SessionHandler.hpp"

SystemInputLink::SystemInputLink(OmniRouter& Router)
    : InputLinkCtx(Router), InputLink(InputLinkCtx), InputFilter(InputLinkCtx),
      InputSynth(InputLink, InputLinkCtx)
{
}

void SystemInputLink::Setup(HWND WindowID)
{
    InputLink.SetupInputLink(WindowID);
}

void SystemInputLink::ToggleEdgeProbe()
{
    InputLink.ToggleEdgeProbe();
}

void SystemInputLink::BindIOLinkSession(SystemLinkContext& Ctx, DeviceMap DeviceID)
{
    if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
        auto& instance = Ctx.ActiveInstances->at(DeviceID);
        Ctx.Router.RegisterSession(DeviceID, instance.InstanceSession.get());
        InputLink.AddEdgeCondition(DeviceID);
        if (!InputLink.GetEdgeProbeState())
            InputLink.ToggleEdgeProbe();

        ToggleInputFilter();
    }
}

void SystemInputLink::UnbindIOLinkSession(SystemLinkContext& Ctx, DeviceMap DeviceID)
{
    Ctx.Router.UnregisterSession(DeviceID);
    if (InputLinkCtx.ActiveRemoteEdge == DeviceID) {
        InputLinkCtx.DeactivateRemoteEdge();
    }

    if (InputLink.CursorOwner.load(std::memory_order_acquire) == DeviceID) {
        InputLink.CursorOwner.store(DeviceMap::C0, std::memory_order_release);
        InputLink.ResetEdgeRelayMask();
    }

    InputLink.ConditionManager.RemoveEdgeCondition(DeviceID);

    ToggleInputFilter();
}

void SystemInputLink::ToggleInputFilter()
{
    if (InputLink.GetEdgeProbeState()) {
        InputFilter.InvokeInputFilter();
    } else {
        InputFilter.ReleaseInputFilter();
    }
}

OmniNet::PoolConfig SystemInputLink::SetState(
    SystemLinkContext& Ctx,
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    (void)SubStreamID;
    (void)Route;
    (void)Context;
    if (Action == FeatureAction::Activate) {
        if (!InputLink.ConditionManager.GetState(DeviceID)) {
            InputLink.SetupInputLink(Ctx.WindowID);
            BindIOLinkSession(Ctx, DeviceID);
            Logger::log("InputLink enabled for DeviceID {:d}", static_cast<int>(DeviceID));
        }
    } else {
        if (InputLink.ConditionManager.GetState(DeviceID) ||
            InputLinkCtx.ActiveRemoteEdge.load(std::memory_order_acquire) == DeviceID ||
            InputLink.CursorOwner.load(std::memory_order_acquire) == DeviceID) {
            UnbindIOLinkSession(Ctx, DeviceID);
            Logger::log("InputLink disabled for DeviceID {:d}", static_cast<int>(DeviceID));
        }
    }
    return OmniNet::PoolConfig{};
}
