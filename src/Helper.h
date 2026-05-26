#ifndef OMNIHELPER_H
#define OMNIHELPER_H

#pragma once
#include <functional>
#include <unordered_map>
#include <variant>

class Variance {
public:
  template <typename Variant, std::size_t... SequenceIndex>
  inline void static VariantDeserializer(Variant &Dest, size_t Index,
                                         std::index_sequence<SequenceIndex...>,
                                         const void *Buffer) {
    ((Index == SequenceIndex &&
      (Dest = *reinterpret_cast<
              const std::variant_alternative_t<SequenceIndex, Variant> *>(
           Buffer),
       true)) ||
     ...);
  }
};

template <typename Type1, typename Type2, typename KeyType> struct FlowMorph {
  std::unordered_map<KeyType, std::function<bool(int, int)>> conditions = {};

  inline void Add(const KeyType &name,
                  std::function<bool(Type1, Type2)> condition) {
    conditions[name] = condition;
  }

  inline void Remove(const KeyType &name) { conditions.erase(name); }

  inline bool Find(const KeyType &name) {
    if (conditions.find(name) == conditions.end()) {
      return false;
    } else {
      return true;
    }
  }
};

#endif
