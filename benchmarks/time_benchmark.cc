#include "cris/core/utils/time.h"

#include <benchmark/benchmark.h>

namespace cris::core {

static void BM_GetTSCTick(benchmark::State& state) {
    unsigned cpuid = 0;
    for ([[maybe_unused]] auto s : state) {
        benchmark::DoNotOptimize(GetTSCTick(cpuid));
    }
}

static void BM_GetSystemTimestampNsec(benchmark::State& state) {
    for ([[maybe_unused]] auto s : state) {
        benchmark::DoNotOptimize(GetSystemTimestampNsec());
    }
}

static void BM_GetUnixTimestampNsec(benchmark::State& state) {
    for ([[maybe_unused]] auto s : state) {
        benchmark::DoNotOptimize(GetUnixTimestampNsec());
    }
}

BENCHMARK(BM_GetTSCTick);
BENCHMARK(BM_GetSystemTimestampNsec);
BENCHMARK(BM_GetUnixTimestampNsec);

}  // namespace cris::core
