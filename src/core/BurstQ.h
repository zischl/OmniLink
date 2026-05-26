#ifndef BURSTQ_H
#define BURSTQ_H

#pragma once
#include <array>

template <typename Type, unsigned int size> struct BurstQ {
  std::array<Type, size> Queue;
  unsigned int Head = 0;
  unsigned int Tail = 0;
  unsigned int _mask = 2;

  BurstQ() { _mask = size; }

  inline void push(const Type &item) {
    Queue[Head] = item;
    Head = (Head + 1) % _mask;
  }

  inline void pop() { Tail = (Tail + 1) % _mask; }
};

#endif // BURSTQ_H
