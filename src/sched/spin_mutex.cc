#include "cris/core/spin_mutex.h"

#include <cstddef>
#include <thread>

#define _ASM_NOOP asm volatile("nop")

// Cannot use `do { } while (0)` here because there would be huge differences between -O0 and -O3
#define _2X(statement) \
    {                  \
        statement;     \
        statement;     \
    }

#define _2X_NOOP _2X(_ASM_NOOP)
#define _4X_NOOP _2X(_2X_NOOP)
#define _8X_NOOP _2X(_4X_NOOP)
#define _16X_NOOP _2X(_8X_NOOP)
#define _32X_NOOP _2X(_16X_NOOP)

namespace cris::core {

void HybridSpinMutex::lock() {
    // Spinning for about 500us@3GHz
    constexpr std::size_t kSpinningCheckLimit = 500;
    for (std::size_t i = 0; i < kSpinningCheckLimit; ++i) {
        if (try_lock()) {
            return;
        }

        constexpr std::size_t kSelfSpinningBetweenChecks = 300;
        // Each iter takes ~3ns on Intel x86 boosting to 2.9-3.0GHz (measured).
        // Tested on Skylake Xeon and CoffeeLake Mobile.
        // Tested on GCC 10/11 and LLVM 12.
        //
        // ~= 1us@3GHz between atomic variable checks.
        for (size_t j = 0; j < kSelfSpinningBetweenChecks; ++j) {
            _32X_NOOP
        }
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
