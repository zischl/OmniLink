#pragma once
#include "PlatformIntrinsics.h"
#include <cassert>
#include <iostream>

void PlatformIntrinsicsTest()
{
    std::cout << "[RUN] PlatformIntrinsicsTest\n";

    // Testing BitScan with single bits set
    for (uint32_t i = 0; i < 32; ++i) {
        uint32_t val = 1U << i;
        uint32_t scanResult = BitScan(val);
        assert(scanResult == i);
    }

    // Testing BitScan with multiple bits set
    // 0b1010 .. lowest bit set is index 1
    assert(BitScan(10) == 1);
    // 0b1100 .. lowest bit set is index 2
    assert(BitScan(12) == 2);
    // 0b10000000 .. lowest bit set is index 7
    assert(BitScan(128) == 7);

    std::cout << "[PASS] PlatformIntrinsicsTest\n";
}
