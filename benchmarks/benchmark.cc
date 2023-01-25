#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#define _CR_USE_SANITIZER 1
#endif
#endif

#ifndef _CR_USE_SANITIZER
#include "job_runner_benchmark.hpp"
#include "time_benchmark.hpp"
#include "timer_benchmark.hpp"
#endif

#include <benchmark/benchmark.h>

BENCHMARK_MAIN();
