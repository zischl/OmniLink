#pragma once
#include "OmniUUID.h"
#include <cassert>
#include <iostream>

void OmniUUIDTest()
{
    std::cout << "[RUN] OmniUUIDTest\n";

    // Generating local IDs
    NodeID id1 = GenerateID();
    NodeID id2 = GenerateID();

    // Verifying comparison operators
    assert(id1 == id1);
    assert(id2 == id2);

    assert(!(id1 == id2));

    assert(id1 == id1.Bytes);

    std::cout << "[PASS] OmniUUIDTest\n";
}
