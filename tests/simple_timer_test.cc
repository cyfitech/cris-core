#include "cris/core/timer/simple_timer.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace std::chrono;
using namespace std::literals::chrono_literals;

namespace cris::core {

TEST(SimpleTimerTest, Init) {
    SimpleTimer timer;

    std::this_thread::sleep_for(1us);

    EXPECT_GT(timer.TimeElapsedNs(), 0);
}

TEST(SimpleTimerTest, Duration) {
    SimpleTimer timer;

    timer.Start();
    std::this_thread::sleep_for(1us);

    EXPECT_GT(timer.TimeElapsedNs(), 0);
}

TEST(SimpleTimerTest, DurationCast) {
    SimpleTimer timer;

    std::this_thread::sleep_for(1ms);

    EXPECT_GT(timer.DurationCast<microseconds>(), 0us);
}

TEST(ScopedTimerTest, Duration) {
    microseconds time_elapsed_us{0};

    {
        ScopedTimer scoped_timer{[&time_elapsed_us](const SimpleTimer& timer) {
            time_elapsed_us = timer.DurationCast<decltype(time_elapsed_us)>();
        }};
        std::this_thread::sleep_for(1ms);
    }

    EXPECT_GT(time_elapsed_us, 0us);
}

}  // namespace cris::core
