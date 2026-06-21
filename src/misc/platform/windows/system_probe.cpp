#include "system_probe_impl.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "OmniLogger.h"
#include "SessionHandler.h"

#include <Windows.h>

Device::MonitorRes Device::GetMonitorResolution()
{
    return MonitorRes{static_cast<uint32_t>(GetSystemMetrics(SM_CXSCREEN)),
                      static_cast<uint32_t>(GetSystemMetrics(SM_CYSCREEN))};
}

void Device::RetrieveUserName(char (&CharArray)[MAX_UNLEN])
{
    wchar_t WUserName[MAX_UNLEN + 1];
    DWORD size = MAX_UNLEN + 1;

    if (!GetUserNameW(WUserName, &size)) {
        CharArray[0] = '\0';
        return;
    }

    WideCharToMultiByte(CP_ACP, 0, WUserName, -1, CharArray, MAX_UNLEN, nullptr, nullptr);

    CharArray[MAX_UNLEN - 1] = '\0';
}

void Device::RetrieveComputerName(char (&CharArray)[MAX_CNLEN])
{
    wchar_t WComputerName[MAX_CNLEN + 1];
    DWORD size = MAX_CNLEN + 1;

    if (!GetComputerNameW(WComputerName, &size)) {
        CharArray[0] = '\0';
        return;
    }

    WideCharToMultiByte(CP_ACP, 0, WComputerName, -1, CharArray, MAX_CNLEN, nullptr, nullptr);

    CharArray[MAX_CNLEN - 1] = '\0';
}

void Device::RetrieveLocalIP(uint32_t& LocalIP, const int index)
{

    std::vector<sockaddr_in> LocalIPs;

    sessions::GetLocals(4, &LocalIPs);
    if (index >= 0 && index < static_cast<int>(LocalIPs.size())) {
        LocalIP = htonl(LocalIPs[index].sin_addr.S_un.S_addr);
        return;
    }

    LocalIP = 0;
    Logger::log("Failed to Retrieve Local IP : Please Check Your Connection!\n");
}
