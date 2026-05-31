#ifndef NETUTILS_H
#define NETUTILS_H

#pragma once

#include <cstdio>
#include <stdint.h>

inline void IP2Char(const uint32_t IP, char* array)
{
    std::sprintf(
        array, "%u.%u.%u.%u", (IP >> 24) & 0xFF, (IP >> 16) & 0xFF, (IP >> 8) & 0xFF, IP & 0xFF);
}

#endif // !NETUTILS_H
