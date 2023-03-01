#include "cris/core/utils/time.h"

#include <gtest/gtest.h>

#include <chrono>

namespace cris::core {

using namespace std::chrono;
using TimePoint = system_clock::time_point;

TEST(TimeTest, IsNotSameDay) {
    constexpr TimePoint start = TimePoint{} + hours{1};
    constexpr TimePoint end   = TimePoint{} + days{1};
    EXPECT_FALSE(SameUtcDay(start, end));
}

TEST(TimeTest, IsSameDay) {
    constexpr auto less_than_one_day = seconds{days::period::num - 1};
    constexpr auto start             = TimePoint{};
    constexpr auto end               = TimePoint{less_than_one_day};
    EXPECT_TRUE(SameUtcDay(start, end));
}

TEST(TimeTest, IsNotSameHour) {
    constexpr auto start = TimePoint{} + minutes{10};
    constexpr auto end   = start + hours{1};
    EXPECT_FALSE(SameUtcHour(start, end));
}

TEST(TimeTest, IsSameHour) {
    constexpr auto less_than_one_hour = seconds{hours::period::num - 1};
    constexpr auto start              = TimePoint{};
    constexpr auto end                = TimePoint{less_than_one_hour};
    EXPECT_TRUE(SameUtcHour(start, end));
}

}  // namespace cris::core
