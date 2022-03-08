#include "cris/core/timer/timer.h"

#ifdef _MSC_VER
#include <intrin.h>
#else
#if __has_include(<x86intrin.h>)
#include <x86intrin.h>
#endif
#endif

#include <chrono>
#include <thread>

namespace cris::core {

static const double kTscToNsecRatio = []() {
    constexpr auto kCalibrationDuration = std::chrono::seconds(1);

    unsigned cpuid = 0;

    const auto start      = std::chrono::steady_clock::now();
    const auto start_tick = GetTSCTick(cpuid);
    std::this_thread::sleep_for(kCalibrationDuration);
    const auto end_tick = GetTSCTick(cpuid);
    const auto end      = std::chrono::steady_clock::now();

    const auto nsec_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return nsec_duration.count() * 1.0 / (end_tick - start_tick);
}();

unsigned long long GetTSCTick(unsigned& aux) {
    /* TODO(xkszltl) RDTSCP is serialized, making it a more robust choice than RDTSC.
     * However it is not supported by Linux docker on macOS (via VM internally).
     * Disable it statically until we have better idea for cheap runtime checks.
     */
    if constexpr (false) {
        return __rdtscp(&aux);
    }
    return __rdtsc();
}

cr_timestamp_nsec_t GetSystemTimestampNsec() {
    unsigned cpuid = 0;
    return static_cast<cr_timestamp_nsec_t>(GetTSCTick(cpuid) * kTscToNsecRatio);
}

cr_timestamp_nsec_t GetUnixTimestampNsec() {
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::system_clock;
    return static_cast<cr_timestamp_nsec_t>(duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace cris::core
