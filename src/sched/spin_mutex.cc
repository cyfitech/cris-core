#include "cris/core/spin_mutex.h"

#include <cstddef>
#include <thread>

#define _ASM_NOP asm volatile("nop")

// Cannot use `do { } while (0)` here because there would be huge differences between -O0 and -O1.
#define _2X(statement) \
    {                  \
        statement;     \
        statement;     \
    }

#define _2X_NOP _2X(_ASM_NOP)
#define _4X_NOP _2X(_2X_NOP)
#define _8X_NOP _2X(_4X_NOP)
#define _16X_NOP _2X(_8X_NOP)
#define _32X_NOP _2X(_16X_NOP)

namespace cris::core {

void HybridSpinMutex::lock() {
    // Spinning for about 500us@3GHz.
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
            _32X_NOP
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
