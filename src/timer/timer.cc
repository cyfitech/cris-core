#include "cris/core/timer/timer.h"

namespace cris::core {

cr_timestamp_nsec_t GetSystemTimestampNsec() {
    using std::chrono::duration_cast;
    using std::chrono::nanoseconds;
    using std::chrono::steady_clock;
    return static_cast<cr_timestamp_nsec_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
}

}  // namespace cris::core
