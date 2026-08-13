#ifndef GUI_TYPES_H
#define GUI_TYPES_H

#pragma once
#include "OmniEnums.h"
#include <imgui.h>
#include <vector>

struct ButtonColors
{
    ImU32 Normal;
    ImU32 Hovered;
    ImU32 Active;
    ImU32 Border;
    ImU32 Text;
    float BorderSize;
};

struct UIDeviceLayout
{
    DeviceMap DeviceID;
    const char* Label;
    float DirectionalityX;
    float DirectionalityY;
    bool DiagonalState;
};

struct MetricItem
{
    const char* Title;
    const char* Value;
};

struct KeybindItem
{
    const char* Name;
    std::vector<const char*> Keys;
};

struct KeybindCategoryGroup
{
    const char* Icon;
    const char* Title;
    std::vector<KeybindItem> Items;
};

#endif // GUI_TYPES_H
