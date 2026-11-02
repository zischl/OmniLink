#pragma once

#include "AudioCap.hpp"
#include "AudioRender.hpp"
#include "OmniEnums.hpp"
#include "OmniPackets.hpp"
#include "SystemLinkContext.h"

#include <array>
#include <atomic>
#include <map>
#include <memory>

template <uint32_t MTU> class OmniNetSession;
class OmniNetSubStream;

struct SystemAudioLink
{
    // AudioStreams and renderers
    OmniAudioLink                                              AudioLink;
    std::array<std::atomic<OmniNetSubStream*>, DeviceMap::END> ActiveAudioStreams{};
    std::atomic<uint32_t>                                      AudioStreamCount{0};
    std::map<SubStreamID, std::unique_ptr<AudioRender>>        AudioRenderers;

    OmniNet::PoolConfig SetState(
        SystemLinkContext& Ctx,
        DeviceMap          DeviceID,
        FeatureActionRoute Route,
        FeatureAction      Action,
        SubStreamID        SubStreamID = 0,
        void*              Context     = nullptr
    );
};
