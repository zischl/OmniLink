#ifndef OMNIHELPER_H
#define OMNIHELPER_H

#pragma once
#include "Logger.h"
#include "OmniTypes.h"
#include <iostream>
#include <variant>


class Variance
{
public:

	template<typename Variant, std::size_t... SequenceIndex>
	inline void static VariantDeserializer(Variant& Dest, size_t Index, std::index_sequence<SequenceIndex ...>, const void* Buffer)
	{
		((Index == SequenceIndex && ( Dest = *reinterpret_cast<const std::variant_alternative_t<SequenceIndex, Variant>*>(Buffer), true)) || ...);
	}
		


};


#endif