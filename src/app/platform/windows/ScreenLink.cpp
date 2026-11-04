#include "ScreenLink.hpp"
#include "CaptureStreams.hpp"
#include "OmniInstances.hpp"
#include "OmniLogger.hpp"
#include "OmniRouterContext.hpp"

OmniStreamer::StreamID SystemScreenLink::StartScreenCaptureStream(
    SystemLinkContext& Ctx, SubStreamID SubStreamID, DeviceMap DeviceID
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
    if (CaptureStreamID == OmniStreamer::InvalidStreamID) {
        return 0;
    }

    Ctx.CaptureStreams.RegisterStreamerID(SubStreamID, CaptureStreamID);

    Logger::log(
        "ScreenCapture started for SubStreamID={:d}, StreamID={:d}", SubStreamID, CaptureStreamID
    );
    return CaptureStreamID;
}

void SystemScreenLink::StopScreenCaptureStream(SystemLinkContext& Ctx, SubStreamID SubStreamID)
{
    Ctx.CaptureStreams.RemoveCaptureStream(SubStreamID);
}

OmniNet::PoolConfig SystemScreenLink::SetState(
    SystemLinkContext& Ctx,
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID
)
{
    if (Route == FeatureActionRoute::Outbound) {
        if (Action == FeatureAction::Activate) {
            StartScreenCaptureStream(Ctx, SubStreamID, DeviceID);
        } else {
            if (SubStreamID != 0) {
                StopScreenCaptureStream(Ctx, SubStreamID);
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance   = Ctx.ActiveInstances->at(DeviceID);
                auto  SubStreams = Instance.GetSubStreams(FeatureTypes::ScreenLink);
                for (uint16_t StreamID : SubStreams) {
                    StopScreenCaptureStream(Ctx, StreamID);
                }
            }
        }
    } else {
        if (Action == FeatureAction::Activate) {
            uint32_t TargetW = 0, TargetH = 0;
            Ctx.Router.GetDeviceResolution(DeviceID, TargetW, TargetH);

            return Ctx.CaptureStreams.SetupStreamRenderWindow(
                Ctx, SubStreamID, DeviceID, TargetW, TargetH, 0, 0, SW_SHOWNORMAL, "Screen Link"
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