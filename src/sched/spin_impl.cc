#include "cris/core/sched/spin_impl.h"

#include <cstddef>

#define _CR_ASM_NOP asm volatile("nop")

// Cannot use `do { } while (0)` here because there would be huge differences between -O0 and -O1.
#define _CR_2X(statement) \
    {                     \
        statement;        \
        statement;        \
    }

#define _CR_2X_NOP _CR_2X(_CR_ASM_NOP)
#define _CR_4X_NOP _CR_2X(_CR_2X_NOP)
#define _CR_8X_NOP _CR_2X(_CR_4X_NOP)
#define _CR_16X_NOP _CR_2X(_CR_8X_NOP)
#define _CR_32X_NOP _CR_2X(_CR_16X_NOP)

namespace cris::core::impl {

void SpinForAbout1us() {
    // Each iter takes ~3ns on Intel x86 boosting to 2.9-3.0GHz (measured).
    // Tested on Skylake Xeon and CoffeeLake Mobile.
    // Tested on GCC 10/11 and LLVM 12.
    //
    // ~= 1us@3GHz between atomic variable checks.
    constexpr std::size_t kSelfSpinningBetweenChecks = 300;
    for (std::size_t i = 0; i < kSelfSpinningBetweenChecks; ++i) {
        _CR_32X_NOP
    }
}

}  // namespace cris::core::impl

#undef _CR_32X_NOP
#undef _CR_16X_NOP
#undef _CR_8X_NOP
#undef _CR_4X_NOP
#undef _CR_2X_NOP
#undef _CR_2X
#undef _CR_ASM_NOP
