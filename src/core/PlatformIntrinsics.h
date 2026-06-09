#ifndef OMNIPLATF_INTRINSICS_H
#define OMNIPLATF_INTRINSICS_H

#pragma once
#include <stdint.h>

#if defined(_MSC_VER) || defined(__MINGW32__)
#include <intrin.h>
inline uint32_t BitScan(uint32_t v)
{
    unsigned long BitIndex;
    _BitScanForward(&BitIndex, v);
    return BitIndex;
}
#else
inline uint32_t BitScan(uint32_t BitIndex)
{
    return __builtin_ctz(BitIndex);
}
#endif

#endif // !OMNIPLATF_INTRINSICS_H
