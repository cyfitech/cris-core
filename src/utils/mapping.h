#pragma once

#include <stdexcept>

namespace cris::core {
// Inverse mapping k -> v to v -> k.
// NOTE: Exception std::logic_error is thrown if the input is not an injection.
template<typename K, typename V, template<typename...> typename Map>
Map<V, K> InverseMapping(const Map<K, V>& original_map) {
    Map<V, K> inversed_map;
    for (const auto& [k, v] : original_map) {
        const auto [iter, ok] = inversed_map.emplace(v, k);
        if (!ok) {
            throw std::logic_error{"Duplicate value."};
        }
    }
    return inversed_map;
}
}  // namespace cris::core
