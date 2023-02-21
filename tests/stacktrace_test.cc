#include "cris/core/signal/cr_signal.h"

#include "gtest/gtest.h"

#include <cstdlib>

namespace cris::core {

__attribute__((noinline)) static void CrashTestInnerFunc() {
    std::abort();
}

__attribute__((noinline)) static void CrashTestFunc() {
    CrashTestInnerFunc();
}

TEST(StacktraceTest, Basics) {
    InstallSignalHandler();

    // Stacktrace should at least include the function names of 2 stack frames.
    EXPECT_DEATH(CrashTestFunc(), "\\bCrashTestInnerFunc().*\n.*\\bCrashTestFunc()");
}

}  // namespace cris::core
