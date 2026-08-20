#ifndef OMNITYPES_H
#define OMNITYPES_H

#pragma once

#include <cstdint>

namespace OmniNet {
enum BufferType : uint8_t { OP_RECV, OP_SEND };

enum PacketType : uint8_t {
    ChunkStart,
    ChunkData,
    ChunkEnd,
    Command,
    ProcMouse,
    ProcKey,
    ProcClipboard
};

enum FlagTypes : uint8_t { VoidArg, Argonized };

} // namespace OmniNet

#endif // OMNITYPES_H
