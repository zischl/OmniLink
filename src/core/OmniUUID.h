#ifndef OMNIUUID_H
#define OMNIUUID_H

#pragma once
#include <algorithm>
#include <cstdint>
#include <random>

struct NodeID
{
    uint8_t Bytes[12];

    bool operator==(const NodeID& other) const
    {
        return std::equal(Bytes, Bytes + 12, other.Bytes);
    }

    bool operator==(const uint8_t* other) const { return std::equal(Bytes, Bytes + 12, other); }
};

inline NodeID GenerateLocalID()
{
    static std::random_device RandomDevice;
    static std::mt19937 Generator(RandomDevice());
    std::uniform_int_distribution<uint16_t> Distribution(0, 255);

    NodeID ID;
    for (int i = 0; i < 12; ++i) {
        ID.Bytes[i] = static_cast<uint8_t>(Distribution(Generator));
    }
    return ID;
}

#endif
