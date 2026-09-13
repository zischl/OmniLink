#ifndef DECODER_CONCEPT_H
#define DECODER_CONCEPT_H

#pragma once
#include <concepts>
#include <d3d11.h>
#include <utility>

// Each decoder must be constructible with width, height, and output D3D11 texture pointer
// And.. must provide a Decode function matching the signature
// The core behind runtime decoder switching from.. ex : Nvidia -> Intel/Amd
template <typename DecoderType>
concept DecoderConcept = requires(
    DecoderType DecoderObj, const unsigned char* Data, unsigned long Size
) {
    { DecoderType(std::declval<UINT>(), std::declval<UINT>(), std::declval<ID3D11Texture2D*>()) };
    { DecoderObj.Decode(Data, Size) } -> std::same_as<void>;
};

#endif // DECODER_CONCEPT_H
