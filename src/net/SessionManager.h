#ifndef OMNISESSIONMGR_H
#define OMNISESSIONMGR_H

#pragma once
#include "OmniConfig.h"
#include "OmniInstances.h"
#include "OmniLogger.h"
#include "SessionHandler.h"

#include <cstdint>
#include <memory>

struct OmniSessionManager
{
    OmniNetContext sessions;
    uint8_t SessionCount = 0;

    template <void (*PacketHandler)(char*, uint32_t, uint8_t, void*), typename PacketContext>
    std::unique_ptr<OmniNetSession<OmniMTU>> Connect(
        const OmniActiveInstance& UserInstance,
        const OmniInstance& TargetInstance,
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

        if (!NetSession->GetSessionState()) {
            Logger::log(
                "OmniNetSession setup failed for {} :: Bind or Connect Error :]",
                TargetInstance.IPv4_String
            );
            return nullptr;
        }

        NetSession->SessionStart<PacketHandler>(Context);

        return NetSession;
    }
};

#endif // !OMNISESSIONMGR_H
