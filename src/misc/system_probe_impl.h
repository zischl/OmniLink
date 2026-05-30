#ifndef SYSTEMPROBE_H
#define SYSTEMPROBE_H

#pragma once
#include <cstdint>

#define MAX_UNLEN 32
#define MAX_CNLEN 63

struct MonitorRes
{
    int Width = 0;
    int Height = 0;
};

namespace Device {
MonitorRes GetMonitorResolution();

void RetrieveUserName(char (&CharArray)[MAX_UNLEN]);

void RetrieveComputerName(char (&CharArray)[MAX_CNLEN]);

void RetrieveLocalIP(uint32_t& LocalIP, const int index);
} // namespace Device

#endif
