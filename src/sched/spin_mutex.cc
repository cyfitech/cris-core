#include "cris/core/sched/spin_mutex.h"

#include "nop_asm_impl.h"

#include <cstddef>
#include <thread>

namespace cris::core {

void HybridSpinMutex::lock() {
    // Spinning for about 500us@3GHz.
    //
    // TODO(hao.chen, xkszltl): Check clock every 10-100us for better accuracy.
    constexpr std::size_t kSpinningCheckLimit = 500;
    for (std::size_t i = 0; i < kSpinningCheckLimit; ++i) {
        if (try_lock()) {
            return;
        }
        _CR_SPIN_FOR_ABOUT_1US_;
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
