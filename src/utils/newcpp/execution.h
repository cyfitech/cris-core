#pragma once

#if __has_include(<execution>)
#include <execution>
#endif

namespace cris::libs::execution {

namespace impl {

void PrintFallbackWarningOnce();

}  // namespace impl

class fallback_policy {};

namespace fallback {

inline constexpr fallback_policy seq;
inline constexpr fallback_policy par;
inline constexpr fallback_policy par_unseq;
inline constexpr fallback_policy unseq;

}  // namespace fallback

}  // namespace cris::libs::execution

// Feature testing macros: https://en.cppreference.com/w/cpp/feature_test#Library_features
#if defined(__cpp_lib_execution)

#if __cpp_lib_execution >= 201603L
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define _CR_HAS_STD_EXECUTION_SEQ 1
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define _CR_HAS_STD_EXECUTION_PAR 1
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define _CR_HAS_STD_EXECUTION_PAR_UNSEQ 1
#endif

#if __cpp_lib_execution >= 201902L
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define _CR_HAS_STD_EXECUTION_UNSEQ 1
#endif

#endif  // defined(__cpp_lib_execution)

namespace std::execution {

#ifndef _CR_HAS_STD_EXECUTION_SEQ
using ::cris::libs::execution::fallback::seq;
#endif

#ifndef _CR_HAS_STD_EXECUTION_PAR
using ::cris::libs::execution::fallback::par;
#endif

#ifndef _CR_HAS_STD_EXECUTION_PAR_UNSEQ
using ::cris::libs::execution::fallback::par_unseq;
#endif

#ifndef _CR_HAS_STD_EXECUTION_UNSEQ
using ::cris::libs::execution::fallback::unseq;
#endif

}  // namespace std::execution
