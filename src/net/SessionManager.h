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
    OmniNetContext sessions;
    uint8_t SessionCount = 0;

    template <typename PacketHandler, typename PacketContext>
    std::unique_ptr<OmniNetSession<OmniMTU>> Connect(
        const OmniActiveInstance& UserInstance,
        const OmniInstance& TargetInstance,
        PacketHandler&& Handler,
        PacketContext* Context
    )
    {
        std::unique_ptr<OmniNetSession<OmniMTU>> NetSession =
            std::make_unique<OmniNetSession<OmniMTU>>(
                UserInstance.IPv4_String,
                TargetInstance.IPv4_String,
                OmniPort,
                Context,
                TargetInstance.DevMapIndex
            );

        NetSession->OnIOCompletion = Handler;

        return NetSession;
    }
};

#endif // !OMNISESSIONMGR_H
