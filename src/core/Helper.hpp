#ifndef OMNIHELPER_H
#define OMNIHELPER_H

#pragma once
#include <functional>
#include <unordered_map>
#include <variant>

class Variance
{
  public:
    template <typename Variant, std::size_t... SequenceIndex>
    inline void static VariantDeserializer(
        Variant& Dest, size_t Index, std::index_sequence<SequenceIndex...>, const void* Buffer
    )
    {
        ((Index == SequenceIndex &&
          (Dest =
               *reinterpret_cast<const std::variant_alternative_t<SequenceIndex, Variant>*>(Buffer),
           true)) ||
         ...);
    }

    template <typename T, typename Variant> struct variant_index;

    template <typename T, typename... Types> struct variant_index<T, std::variant<Types...>>
    {
        static constexpr std::size_t value = []() {
            std::size_t index = 0;
            bool found = ((std::is_same_v<T, Types> ? true : (++index, false)) || ...);
            return found ? index : static_cast<std::size_t>(-1);
        }();
    };

    template <typename T, typename Variant>
    static inline constexpr std::size_t GetVariantTypeIndex = variant_index<T, Variant>::value;
};

template <typename Type1, typename Type2, typename KeyType> struct FlowMorph
{
    std::unordered_map<KeyType, std::function<bool(int, int)>> conditions = {};

    inline void Add(const KeyType& name, std::function<bool(Type1, Type2)> condition)
    {
        conditions[name] = condition;
    }

    inline void Remove(const KeyType& name) { conditions.erase(name); }

    inline bool Find(const KeyType& name) { return conditions.find(name) != conditions.end(); }

    inline bool Empty() const { return conditions.empty(); }

    inline std::size_t Size() const { return conditions.size(); }
};

#endif
