#ifndef GUI_ENUMS_H
#define GUI_ENUMS_H

#pragma once

enum class GroupCardAction
{
    None,
    ConnectSettings,
    DeleteGroup
};

enum class ActiveCardAction
{
    None,
    Connect,
    Disconnect,
    ConfigureSettings
};

enum class TrustedCardAction
{
    None,
    Connect,
    Forget
};

#endif // GUI_ENUMS_H
