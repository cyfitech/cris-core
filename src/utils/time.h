#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace cris::core {

using cr_timestamp_nsec_t = std::int64_t;
using cr_duration_nsec_t  = std::int64_t;

// CPU Time Stamp Counter tick.
unsigned long long GetTSCTick(unsigned& aux);

// Monotonic timestamp.
cr_timestamp_nsec_t GetSystemTimestampNsec();

// Real-world timestamp.
cr_timestamp_nsec_t GetUnixTimestampNsec();

// e.g., 20230224
std::string GetCurrentUtcDate();

// e.g., 2023022407
std::string GetCurrentUtcHour();

bool SameUtcDay(const std::chrono::system_clock::time_point x, const std::chrono::system_clock::time_point y) noexcept;

bool SameUtcHour(const std::chrono::system_clock::time_point x, const std::chrono::system_clock::time_point y) noexcept;

}  // namespace cris::core
