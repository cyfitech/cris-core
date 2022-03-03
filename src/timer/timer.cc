#include "cris/core/timer/timer.h"

#include <x86intrin.h>

#include <chrono>
#include <thread>

namespace cris::core {

static const double kTscToNsecRatio = []() {
    constexpr auto kCalibrationDuration = std::chrono::seconds(1);
    const auto     start                = std::chrono::steady_clock::now();
    const auto     start_tick           = __rdtsc();
    std::this_thread::sleep_for(kCalibrationDuration);
    const auto end_tick = __rdtsc();
    const auto end      = std::chrono::steady_clock::now();

    const auto nsec_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    return nsec_duration.count() * 1.0 / (end_tick - start_tick);
}();

cr_timestamp_nsec_t GetSystemTimestampNsec() {
    return static_cast<cr_timestamp_nsec_t>(__rdtsc() * kTscToNsecRatio);
}

cr_timestamp_nsec_t GetUnixTimestampNsec() {
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::system_clock;
    return static_cast<cr_timestamp_nsec_t>(duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace cris::core
