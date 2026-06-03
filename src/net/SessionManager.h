#ifndef OMNISESSIONMGR_H
#define OMNISESSIONMGR_H

#pragma once
#include "OmniInstances.h"
#include "OmniPackets.h"
#include "SessionHandler.h"

#define OmniPort 62485
#define MTU 1450

struct SessionManager
{
    sessions sessions;
    uint8_t SessionCount = 0;

    template <typename PacketHandler, typename PacketContext>
    session* Connect(const ConnectionRequest request,
                     const OmniActiveInstance& UserInstance,
                     const OmniActiveInstance TargetInstance,
                     PacketHandler&& Handler,
                     PacketContext* Context)
    {
        session* NetSession = new session(sessions.IOCP,
                                          UserInstance.IPv4_String,
                                          TargetInstance.IPv4_String,
                                          TargetInstance.port,
                                          MTU,
                                          Context);
        Logger::log(
            "Connecting to : ", TargetInstance.InstanceName, "at ", TargetInstance.IPv4_String);

        NetSession->OnIOCompletion = Handler;

        return NetSession;
    }
};

#endif // !OMNISESSIONMGR_H
