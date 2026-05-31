#ifndef NETVARIANCE_H
#define NETVARIANCE_H

#pragma once
#include "ByteStream.h"
#include <variant>

template <typename Variant, std::size_t... SequenceIndex>
inline void static NetVariantDeserializer(Variant& Dest,
                                          size_t Index,
                                          std::index_sequence<SequenceIndex...>,
                                          uint8_t* Buffer,
                                          const uint32_t BufferLen)
{
    ByteStreamReader Reader{BufferLen, Buffer};
    ((Index == SequenceIndex &&
      (Dest = std::variant_alternative_t<SequenceIndex, Variant>::Deserialize(Reader), true)) ||
     ...);
}

/*template<typename Variant, std::size_t... SequenceIndex>
inline void static NetVariantSerializer(Variant& Dest, size_t Index,
std::index_sequence<SequenceIndex ...>, const uint8_t* Buffer, const uint32_t
BufferLen)
{
        ByteStreamReader Reader{ BufferLen, Buffer };
        ((Index == SequenceIndex && (Dest =
std::variant_alternative_t<SequenceIndex, Variant>::Serialize(Reader), true))
|| ...);
}*/

#endif // !NETVARIANCE_H
