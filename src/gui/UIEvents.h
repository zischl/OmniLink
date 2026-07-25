#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#pragma once
#include "OmniEnums.h"

#include <format>
#include <string_view>
#include <variant>

struct Alert
{
    char Title[32] = {};
    char Desc[64] = {};

    Alert() = default;

    Alert(std::string_view title, std::string_view descPrefix, std::string_view descValue)
    {
        title.copy(Title, sizeof(Title) - 1);

        std::format_to_n(Desc, sizeof(Desc) - 1, "{}{}", descPrefix, descValue);
    }
};

struct HandshakeWaitEvent
{
    DeviceMap DeviceID = DeviceMap::END;
    char VerificationCode[7]{'0', '0', '0', '0', '0', '0', '\0'};
    char InstanceName[32]{};
};

struct HandshakeConfirmEvent : public HandshakeWaitEvent
{
    bool Trusted = false;
};

using EventData = std::variant<Alert, HandshakeWaitEvent, HandshakeConfirmEvent>;

struct Notification
{
    EventData Event;
    const char* EventName;
    enum EventLayout { CENTER, BOTTOM_RIGHT } Layout = EventLayout::CENTER;
    float Timeout = 15.0f;
    bool Active = true;
};

#endif // !UI_EVENTS_H
