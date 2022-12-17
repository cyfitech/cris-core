#pragma once

#include "cris/core/utils/logging.h"

namespace cris::core {
// Inverse mapping k -> v to v -> k
// NOTE: keep the first entry for dupilcate v
template<typename K, typename V, template<typename K1, typename V1> typename Map>
Map<V, K> InverseMapping(const Map<K, V>& original_map) {
    Map<V, K> inversed_map;
    for (const auto& [k, v] : original_map) {
        auto [_, ok] = inversed_map.emplace(v, k);
        RAW_CHECK(ok, "duplicate value");
    }
    return inversed_map;
}
}  // namespace cris::core
