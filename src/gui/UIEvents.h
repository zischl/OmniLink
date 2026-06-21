#ifndef UI_EVENTS_H
#define UI_EVENTS_H

#pragma once
#include "OmniPackets.h"

#include <string>
#include <variant>

struct Alert
{
    std::string Title;
    std::string Desc;
};

using EventData = std::variant<ConnectionRequest, Alert>;

struct Notification
{
    EventData Event;
    const char* EventName;
    enum EventLayout { CENTER, BOTTOM_RIGHT } Layout = EventLayout::CENTER;
    float Timeout = 15.0f;
    bool Active = false;
};

#endif // !UI_EVENTS_H
