#include "cris/core/utils/time.h"

#if defined(__APPLE__) && defined(__MACH__) && __has_include(<mach/mach_time.h>)
#define _CRIS_MACH_TIME 1
#include <mach/mach_time.h>
#elif _MSC_VER
#define _CRIS_TIMER_RDTSC 1
#include <intrin.h>
#elif (defined(__i386__) || defined(__x86_64__) || defined(__amd64__)) && __has_include(<x86intrin.h>)
#define _CRIS_TIMER_RDTSC 1
#include <x86intrin.h>
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

    return static_cast<double>(nsec_duration.count()) / static_cast<double>((end_tick - start_tick));
}();

unsigned long long GetTSCTick([[maybe_unused]] unsigned& aux) {
#if defined(_CRIS_TIMER_RDTSC)
    /* TODO(xkszltl) RDTSCP is serialized, making it a more robust choice than RDTSC.
     * However it is not supported by Linux docker on macOS (via VM internally).
     * Disable it statically until we have better idea for cheap runtime checks.
     */
    if constexpr (false) {
        return __rdtscp(&aux);
    }
    return __rdtsc();
#elif defined(_CRIS_MACH_TIME)
    return mach_absolute_time();
#endif
}

cr_timestamp_nsec_t GetSystemTimestampNsec() {
    unsigned cpuid = 0;
    return static_cast<cr_timestamp_nsec_t>(static_cast<double>(GetTSCTick(cpuid)) * kTscToNsecRatio);
}

cr_timestamp_nsec_t GetUnixTimestampNsec() {
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::system_clock;
    return static_cast<cr_timestamp_nsec_t>(duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace cris::core
