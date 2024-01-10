#pragma once

#include "cris/core/utils/newcpp/execution.h"

#include <algorithm>
#include <execution>
#include <utility>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define _CRIS_ALGORITHM_DEFINE_ADAPTER(ns, func)                                      \
    template<typename... args_t>                                                      \
    decltype(auto) func(::cris::libs::execution::fallback_policy, args_t&&... args) { \
        ::cris::libs::execution::impl::PrintFallbackWarningOnce();                    \
        return ns::func(std::forward<args_t>(args)...);                               \
    }

namespace std {

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, all_of)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, any_of)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, none_of)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, for_each)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, for_each_n)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, count)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, count_if)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, mismatch)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, find)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, find_if)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, find_if_not)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, find_end)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, find_first_of)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, adjacent_find)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, search)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, search_n)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, copy)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, copy_if)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, copy_n)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, move)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, fill)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, fill_n)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, transform)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, generate)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, generate_n)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, remove)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, remove_if)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, remove_copy)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, remove_copy_if)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, replace_copy)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, replace_copy_if)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, swap_ranges)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, reverse)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, reverse_copy)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, rotate)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, rotate_copy)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, unique)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, unique_copy)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, partition)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, partition_copy)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, stable_partition)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, is_sorted)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, is_sorted_until)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, sort)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, partial_sort)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, partial_sort_copy)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, stable_sort)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, nth_element)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, merge)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, inplace_merge)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, includes)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, set_difference)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, set_intersection)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, set_symmetric_difference)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, set_union)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, is_heap)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, is_heap_until)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, max_element)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, min_element)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, minmax_element)

_CRIS_ALGORITHM_DEFINE_ADAPTER(std, equal)
_CRIS_ALGORITHM_DEFINE_ADAPTER(std, lexicographical_compare)

}  // namespace std

#undef _CRIS_ALGORITHM_DEFINE_ADAPTER
