#include "AudioLink.h"
#include "OmniInstances.h"
#include "OmniLogger.hpp"
#include "SessionHandler.hpp"
#include "SubStream.hpp"

OmniNet::PoolConfig SystemAudioLink::SetState(
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
        Logger::log(
            "{:s} AudioLink for DeviceID {:d}, SubStreamID={:d}",
            Action == FeatureAction::Activate ? "Starting" : "Stopping",
            static_cast<int>(DeviceID),
            SubStreamID
        );

        if (Action == FeatureAction::Activate) {
            OmniNetSubStream* TargetSubStream = nullptr;
            if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID) &&
                SubStreamID != 0) {
                auto&           Instance = Ctx.ActiveInstances->at(DeviceID);
                SubStreamEntry* Entry    = Instance.FindSubStream(SubStreamID);
                if (Entry && Entry->SubStream) {
                    TargetSubStream = Entry->SubStream;
                }
            }

            if (TargetSubStream && DeviceID < DeviceMap::END) {
                if (!AudioLink.GetCaptureThreadState()) {
                    if (AudioLink.Init(AudioCaptureMode::DesktopOnly)) {
                        AudioLink.SetPacketCallback(
                            [this](
                                const uint8_t* Data, size_t Size, const AudioFrameHeader& Header
                            ) {
                                (void)Header;
                                for (auto& SubStreamSlot : ActiveAudioStreams) {
                                    auto* SubStream = SubStreamSlot.load(std::memory_order_acquire);
                                    if (SubStream) {
                                        SubStream->ChunkedSend(
                                            reinterpret_cast<CHAR*>(const_cast<uint8_t*>(Data)),
                                            static_cast<int>(Size)
                                        );
                                    }
                                }
                            }
                        );
                        AudioLink.Start();
                    }
                }

                auto* PrevStream = ActiveAudioStreams[DeviceID].exchange(
                    TargetSubStream, std::memory_order_acq_rel
                );
                if (!PrevStream) {
                    AudioStreamCount.fetch_add(1, std::memory_order_acq_rel);
                }
            }
        } else {
            if (DeviceID < DeviceMap::END) {
                if (ActiveAudioStreams[DeviceID].exchange(nullptr, std::memory_order_acq_rel) !=
                    nullptr) {
                    if (AudioStreamCount.fetch_sub(1, std::memory_order_acq_rel) <= 1) {
                        AudioLink.Stop();
                    }
                }
            }
        }
    } else {
        Logger::log(
            "{:s} AudioLink for DeviceID {:d}, SubStreamID={:d}",
            Action == FeatureAction::Activate ? "Starting" : "Stopping",
            static_cast<int>(DeviceID),
            SubStreamID
        );

        if (Action == FeatureAction::Activate) {
            auto Renderer = std::make_unique<AudioRender>();
            if (Renderer->Init()) {
                Renderer->Start();
                OmniNet::PoolConfig Config{};
                Renderer->GetBufferPool(
                    Config.Data,
                    Config.DataSize,
                    Config.NumSlots,
                    &Config.OnSlotComplete,
                    Config.Ctx
                );
                if (SubStreamID != 0) {
                    AudioRenderers[SubStreamID] = std::move(Renderer);
                }
                return Config;
            }
        } else {
            if (SubStreamID != 0) {
                auto Iter = AudioRenderers.find(SubStreamID);
                if (Iter != AudioRenderers.end()) {
                    if (Iter->second) {
                        Iter->second->Stop();
                    }
                    AudioRenderers.erase(Iter);
                }
            } else if (Ctx.ActiveInstances && Ctx.ActiveInstances->contains(DeviceID)) {
                auto& Instance = Ctx.ActiveInstances->at(DeviceID);
                auto  Streams  = Instance.GetSubStreams(FeatureTypes::AudioLink);
                for (auto StreamID : Streams) {
                    auto Iter = AudioRenderers.find(StreamID);
                    if (Iter != AudioRenderers.end()) {
                        if (Iter->second) {
                            Iter->second->Stop();
                        }
                        AudioRenderers.erase(Iter);
                    }
                }
            }
        }
    }
    return OmniNet::PoolConfig{};
}
