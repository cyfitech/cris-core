#include "cris/core/utils/time.h"

#if defined(__APPLE__) && defined(__MACH__) && __has_include(<mach/mach_time.h>)
#define _CR_USE_MACH_TIME 1
#include <mach/mach_time.h>
#elif _MSC_VER
#define _CR_USE_RDTSC 1
#include <intrin.h>
#elif (defined(__i386__) || defined(__x86_64__) || defined(__amd64__)) && __has_include(<x86intrin.h>)
#define _CR_USE_RDTSC 1
#include <x86intrin.h>
#endif

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fmt/core.h>

#include <chrono>
#include <cstdint>
#include <thread>

namespace cris::core {

const double kTscToNsecRatio = []() {
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
#if defined(_CR_USE_MACH_TIME)
    return mach_absolute_time();
#elif defined(_CR_USE_RDTSC)
    /* TODO(xkszltl): RDTSCP is serialized, making it a more robust choice than RDTSC.
     * However it is not supported by Linux docker on macOS (via VM internally).
     * Disable it statically until we have better idea for cheap runtime checks.
     */
    // NOLINTNEXTLINE(readability-simplify-boolean-expr,-warnings-as-errors)
    if constexpr (false) {
        return __rdtscp(&aux);
    }
    return __rdtsc();
#elif defined(__aarch64__)
    // https://github.com/google/benchmark/blob/7d48eff772e0f04761033a4ad2a004b4546df6f8/src/cycleclock.h#L141
    //
    // System timer of ARMv8 runs at a different frequency than the CPU's.
    // The frequency is fixed, typically in the range 1-50MHz.  It can be
    // read at CNTFRQ special register.  We assume the OS has set up
    // the virtual timer properly.
    std::uint64_t virtual_timer_value = 0;
    asm volatile("mrs %0, cntvct_el0" : "=r"(virtual_timer_value));
    return static_cast<unsigned long long>(virtual_timer_value);
#else
#error("Unknown platform.")
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

std::string GetCurrentUtcDate() {
    using namespace boost::gregorian;

    return to_iso_string(day_clock::universal_day());
}

std::string GetCurrentUtcHour() {
    using namespace boost::gregorian;
    using namespace boost::posix_time;

    const auto utc_time = second_clock::universal_time();
    return fmt::format("{}{:02}", to_iso_string(utc_time.date()), utc_time.time_of_day().hours());
}

std::string GetCurrentUtcTime() {
    return boost::posix_time::to_iso_string(boost::posix_time::second_clock::universal_time());
}

bool SameUtcDay(const std::chrono::system_clock::time_point x, const std::chrono::system_clock::time_point y) noexcept {
    using std::chrono::days;
    using std::chrono::duration_cast;

    const auto days_x = duration_cast<days>(x.time_since_epoch()).count();
    const auto days_y = duration_cast<days>(y.time_since_epoch()).count();
    return days_x == days_y;
}

bool SameUtcHour(
    const std::chrono::system_clock::time_point x,
    const std::chrono::system_clock::time_point y) noexcept {
    using std::chrono::duration_cast;
    using std::chrono::hours;

    const auto hours_x = duration_cast<hours>(x.time_since_epoch()).count();
    const auto hours_y = duration_cast<hours>(y.time_since_epoch()).count();
    return hours_x == hours_y;
}

}  // namespace cris::core
