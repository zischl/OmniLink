#ifndef BURSTQ_H
#define BURSTQ_H

#pragma once
#include <array>
#include <atomic>
#include <new>

template <typename Type, uint32_t Size> struct BurstQ
{
    static_assert((Size & (Size - 1)) == 0, "N must be a power of 2");
    std::array<Type, Size> Queue{};
    alignas(64) std::atomic<uint32_t> Head{0};
    alignas(64) std::atomic<uint32_t> Tail{0};

    [[nodiscard]] bool push(const Type& item)
    {
        uint32_t h = Head.load(std::memory_order_relaxed);
        if (((h + 1) & (Size - 1)) == Tail.load(std::memory_order_acquire))
            return false;
        Queue[h] = item;
        Head.store((h + 1) & (Size - 1), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool pop(Type& out)
    {
        uint32_t t = Tail.load(std::memory_order_relaxed);
        if (t == Head.load(std::memory_order_acquire))
            return false;
        out = std::move(Queue[t]);
        Tail.store((t + 1) & (Size - 1), std::memory_order_release);
        return true;
    }
};

#endif // BURSTQ_H
