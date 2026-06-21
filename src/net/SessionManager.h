#ifndef OMNISESSIONMGR_H
#define OMNISESSIONMGR_H

#pragma once
#include "OmniConfig.h"
#include "OmniInstances.h"
#include "OmniPackets.h"
#include "SessionHandler.h"
#include <memory>

struct OmniSessionManager
{
    sessions sessions;
    uint8_t SessionCount = 0;

    template <typename PacketHandler, typename PacketContext>
    std::unique_ptr<session> Connect(
        const OmniActiveInstance& UserInstance,
        const OmniActiveInstance& TargetInstance,
        PacketHandler&& Handler,
        PacketContext* Context
    )
    {
        std::unique_ptr<session> NetSession = std::make_unique<session>(
            UserInstance.IPv4_String,
            TargetInstance.IPv4_String,
            TargetInstance.port,
            OmniMTU,
            Context,
            TargetInstance.DevMapIndex
        );

        NetSession->OnIOCompletion = Handler;

        return NetSession;
    }
};

#endif // !OMNISESSIONMGR_H
