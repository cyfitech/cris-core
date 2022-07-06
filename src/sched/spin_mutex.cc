#include "cris/core/spin_mutex.h"

#include <cstddef>
#include <thread>

namespace cris::core {

void HybridSpinMutex::lock() {
    // 50k loop ~= 500k x86 inst ~= 1M cycles ~= 500us@2GHz
    // To reduce contention, 100 loop ~= 1k x86 inst ~= 2k cycles ~= 1us@2GHz between atomic variable checks.
    constexpr std::size_t kSpinningLimit             = 50000;
    constexpr std::size_t kSelfSpinningBetweenChecks = 100;

    for (std::size_t i = 0; i < kSpinningLimit / kSelfSpinningBetweenChecks; ++i) {
        if (try_lock()) {
            return;
        }
        for (size_t j = 0; j < kSelfSpinningBetweenChecks; ++j) {}
    }

    while (!try_lock()) {
        std::this_thread::yield();
    }
}

void HybridSpinMutex::unlock() {
    locked_.clear();
}

bool HybridSpinMutex::try_lock() {
    return !locked_.test_and_set();
}

}  // namespace cris::core
