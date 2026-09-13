#ifndef NETUTILS_H
#define NETUTILS_H

#pragma once

#include <cstdio>
#include <stdint.h>

inline void IP2Char(const uint32_t IP, char* array)
{
    std::sprintf(
        array, "%u.%u.%u.%u", (IP >> 24) & 0xFF, (IP >> 16) & 0xFF, (IP >> 8) & 0xFF, IP & 0xFF
    );
}

inline uint32_t Char2IP(const char* ipStr)
{
    if (!ipStr || ipStr[0] == '\0')
        return 0;
    unsigned int a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(ipStr, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        return (a << 24) | (b << 16) | (c << 8) | d;
    }
    return 0;
}

#endif // !NETUTILS_H
