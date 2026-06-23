#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#include <string_view>
#pragma once
#include "OmniPackets.h"

#include <variant>

struct Alert
{
    char Title[32];
    char Desc[64];

    Alert(const std::string_view Title_, const std::string_view Desc_)
    {
        std::snprintf(Title, sizeof(Title), "%s", Title_.data());
        std::snprintf(Desc, sizeof(Desc), "%s", Desc_.data());
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
