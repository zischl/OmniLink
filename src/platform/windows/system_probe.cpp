
#include "system_probe_impl.h"

MonitorRes Device::GetMonitorResolution()
{
	return MonitorRes{ GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}