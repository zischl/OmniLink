#include "ScreenLink.h"
#include "OmniInstances.h"
#include "OmniLogger.hpp"
#include "OmniRouterContext.hpp"
#include "SystemLink.h"
#include "CaptureStreams.h"

OmniStreamer::StreamID SystemScreenLink::StartScreenCaptureStream(
    OmniSystemLink* SysLink, SystemLinkContext& Ctx, SubStreamID SubStreamID, DeviceMap DeviceID
)
{
    if (!Ctx.ActiveInstances || !Ctx.ActiveInstances->contains(DeviceID)) {
        return 0;
    }

    auto&           Instance = Ctx.ActiveInstances->at(DeviceID);
    SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
    if (!Entry || !Entry->SubStream) {
        return 0;
    }

    StreamConfig Config{};

    OmniStreamer::StreamID CaptureStreamID = Ctx.CaptureStreams.AddCaptureStream(
        Ctx.GraphicsContext, Entry->SubStream, DeviceID, CaptureMode::DXGI, Config
    );
    if (CaptureStreamID == 0) {
        return 0;
    }

    Ctx.CaptureStreams.StreamerIDRegistry[SubStreamID] = CaptureStreamID;

    Logger::log(
        "ScreenCapture started for SubStreamID={:d}, StreamID={:d}", SubStreamID, CaptureStreamID
    );
    return CaptureStreamID;
}

void SystemScreenLink::StopScreenCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID)
{
    auto IterStreamID = Ctx.CaptureStreams.StreamerIDRegistry.find(SubStreamID);
    if (IterStreamID != Ctx.CaptureStreams.StreamerIDRegistry.end()) {
        Ctx.CaptureStreams.Streamer.RemoveStream(IterStreamID->second);
        Ctx.CaptureStreams.StreamerIDRegistry.erase(IterStreamID);
    }
}

OmniNet::PoolConfig SystemScreenLink::SetState(
    OmniSystemLink*    SysLink,
    SystemLinkContext& Ctx,
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    (void)Context;
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            StartScreenCaptureStream(SysLink, Ctx, SubStreamID, DeviceID);
        } else {
            if (SubStreamID != 0) {
                StopScreenCaptureStream(Ctx, SubStreamID);
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance   = Ctx.ActiveInstances->at(DeviceID);
                auto  SubStreams = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t SubStreamID : SubStreams) {
                    StopScreenCaptureStream(Ctx, SubStreamID);
                }
            }
        }
    } else {
        if (Action == FeatureAction::Activate) {
            uint32_t TargetW = 0, TargetH = 0;
            Ctx.Router.GetDeviceResolution(DeviceID, TargetW, TargetH);

            return Ctx.CaptureStreams.SetupStreamRenderWindow(
                SysLink,
                Ctx,
                SubStreamID,
                DeviceID,
                TargetW,
                TargetH,
                0,
                0,
                SW_SHOWNORMAL,
                "Screen Link"
            );
        } else {
            if (SubStreamID != 0) {
                Ctx.CaptureStreams.DestroyStreamRenderWindow(SubStreamID);
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance = Ctx.ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t StreamID : Streams) {
                    Ctx.CaptureStreams.DestroyStreamRenderWindow(StreamID);
                }
            }
            Logger::log(
                "ScreenLink stopped for device {:d}, SubStreamID={:d}",
                static_cast<int>(DeviceID),
                SubStreamID
            );
        }
    }
    return OmniNet::PoolConfig{};
}
