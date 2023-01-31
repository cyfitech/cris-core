#if defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer) || __has_feature(undefined_behavior_sanitizer)
#define _CR_USE_SANITIZER 1
#endif
#endif

#include "cris/core/signal/cr_signal.h"
#include "cris/core/utils/logging.h"

#include <benchmark/benchmark.h>

int main(int argc, char** argv) {
    // Mute INFO logs.
    FLAGS_minloglevel = 1;

    cris::core::InstallSignalHandler();

    benchmark::Initialize(&argc, argv);
#if !defined(_CR_USE_SANITIZER) || !_CR_USE_SANITIZER
    benchmark::RunSpecifiedBenchmarks();

    // Not available until Benchmark v1.5.5
    // benchmark::Shutdown();
#endif
}
