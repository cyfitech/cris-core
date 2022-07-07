#pragma once

#include <cstdint>

namespace cris::core {

using cr_timestamp_nsec_t = std::int64_t;
using cr_duration_nsec_t  = std::int64_t;

// CPU timestamp counter tick.
unsigned long long GetTSCTick(unsigned& aux);

// Monotonic timestamp.
cr_timestamp_nsec_t GetSystemTimestampNsec();

// Real-world timestamp.
cr_timestamp_nsec_t GetUnixTimestampNsec();

}  // namespace cris::core
