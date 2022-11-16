#pragma once

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

// Cannot use `do { } while (0)` here because there would be huge differences between -O0 and -O1.
// Each iter takes ~3ns on Intel x86 boosting to 2.9-3.0GHz (measured).
// Tested on Skylake Xeon and CoffeeLake Mobile.
// Tested on GCC 10/11 and LLVM 12.
//
// ~= 1us@3GHz between atomic variable checks.
#define _CR_SPIN_FOR_ABOUT_1US_                                   \
    {                                                             \
        constexpr std::size_t kSelfSpinningBetweenChecks = 300;   \
        for (size_t j = 0; j < kSelfSpinningBetweenChecks; ++j) { \
            _32X_NOP                                              \
        }                                                         \
    }
