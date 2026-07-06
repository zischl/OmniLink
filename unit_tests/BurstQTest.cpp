#pragma once
#include "BurstQ.h"
#include <cassert>
#include <iostream>

void BurstQTest()
{
    std::cout << "[RUN] BurstQTest\n";

    BurstQ<int, 8> queue;

    // Verifying initial state
    assert(queue.empty());
    assert(queue.peek() == nullptr);

    for (int i = 0; i < 7; ++i) {
        bool pushed = queue.push(i);
        assert(pushed);
    }

    // Pushing 8th item should fail.. hopefully
    bool pushedOverLimit = queue.push(7);
    assert(!pushedOverLimit);

    // Checking peek on full queue
    int* peeked = queue.peek();
    assert(peeked != nullptr);
    assert(*peeked == 0);

    // uh.. pop..
    for (int i = 0; i < 7; ++i) {
        int val = -1;
        bool popped = queue.pop(val);
        assert(popped);
        assert(val == i);
    }

    // Better be empty now
    assert(queue.empty());
    assert(queue.peek() == nullptr);

    // Can't.. pop air..
    int dummy = 0;
    assert(!queue.pop(dummy));
    assert(!queue.pop());

    std::cout << "[PASS] BurstQTest\n";
}
