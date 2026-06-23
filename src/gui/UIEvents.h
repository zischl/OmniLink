#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include <string_view>
#pragma once
#include "OmniPackets.h"

#include <variant>

struct Alert
{
    char Title[32] = {};
    char Desc[64] = {};

    Alert(std::string_view title, std::string_view descPrefix, std::string_view descValue)
    {
        title.copy(Title, sizeof(Title) - 1);

        std::format_to_n(Desc, sizeof(Desc) - 1, "{}{}", descPrefix, descValue);
    }
};

using EventData = std::variant<ConnectionRequest, Alert>;

struct Notification
{
    EventData Event;
    const char* EventName;
    enum EventLayout { CENTER, BOTTOM_RIGHT } Layout = EventLayout::CENTER;
    float Timeout = 15.0f;
    bool Active = true;
};

#endif // !UI_EVENTS_H
