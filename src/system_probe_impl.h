#ifndef SYSTEMPROBE_H
#define SYSTEMPROBE_H

#pragma once

#include <Windows.h>


struct MonitorRes {
	int Width = 0;
	int Height = 0;

};


namespace Device
{
	MonitorRes GetMonitorResolution();
}


#endif