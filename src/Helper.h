#ifndef OMNIHELPER_H
#define OMNIHELPER_H

#pragma once
#include "Logger.h"
#include "OmniTypes.h"
#include <iostream>
#include <variant>
#include <unordered_map>
#include <mutex>
#include <functional>


class Variance
{
public:

	template<typename Variant, std::size_t... SequenceIndex>
	inline void static VariantDeserializer(Variant& Dest, size_t Index, std::index_sequence<SequenceIndex ...>, const void* Buffer)
	{
		((Index == SequenceIndex && ( Dest = *reinterpret_cast<const std::variant_alternative_t<SequenceIndex, Variant>*>(Buffer), true)) || ...);
	}
		


};

template <typename Type1, typename Type2>
class FlowMorph {
public:
	void Add(const std::string& name, std::function<bool(Type1, Type2)> condition)
	{
		conditions[name] = condition;
	}

	void Remove(const std::string& name)
	{
		conditions.erase(name);
	}


private:
	std::unordered_map<std::string, std::function<bool(int, int)>> conditions;
};


#endif