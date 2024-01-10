#pragma once

#include "cris/core/utils/newcpp/execution.h"

#include <execution>
#include <numeric>
#include <utility>

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define _CRIS_NUMERIC_DEFINE_ADAPTER(ns, func)                                        \
    template<typename... args_t>                                                      \
    decltype(auto) func(::cris::libs::execution::fallback_policy, args_t&&... args) { \
        ::cris::libs::execution::impl::PrintFallbackWarningOnce();                    \
        return ns::func(std::forward<args_t>(args)...);                               \
    }

namespace std {

_CRIS_NUMERIC_DEFINE_ADAPTER(std, adjacent_difference)
_CRIS_NUMERIC_DEFINE_ADAPTER(std, reduce)
_CRIS_NUMERIC_DEFINE_ADAPTER(std, exclusive_scan)
_CRIS_NUMERIC_DEFINE_ADAPTER(std, inclusive_scan)
_CRIS_NUMERIC_DEFINE_ADAPTER(std, transform_reduce)
_CRIS_NUMERIC_DEFINE_ADAPTER(std, transform_exclusive_scan)
_CRIS_NUMERIC_DEFINE_ADAPTER(std, transform_inclusive_scan)

}  // namespace std

#undef _CRIS_NUMERIC_DEFINE_ADAPTER
