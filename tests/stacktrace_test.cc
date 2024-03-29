#include "cris/core/signal/cr_signal.h"
#include "cris/core/utils/logging.h"

#include "gtest/gtest.h"

#include <cstdlib>

namespace cris::core {

__attribute__((noinline)) static void CrashTestInnerFunc() {
    LOG(INFO) << __func__ << ": Generate side-effect to prevent this function from being optimized away.";
    std::abort();
}

__attribute__((noinline)) static void CrashTestFunc() {
    LOG(INFO) << __func__ << ": Generate side-effect to prevent this function from being optimized away.";
    CrashTestInnerFunc();
}

TEST(StacktraceTest, Basics) {
    InstallSignalHandler();

    // Stacktrace should at least include the function names of 2 stack frames.
    // EXPECT_DEATH contains `goto`.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(CrashTestFunc(), "cris::core::CrashTestInnerFunc().*\n.*cris::core::CrashTestFunc()");
}

}  // namespace cris::core
