#include "cris/core/msg_recorder/rolling_helper.h"

#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <stdexcept>

using namespace std::chrono;

namespace cris::core {

using TimePoint = RollingHelper::Metadata::TimePoint;

class RollingByDayTestHelper : public RollingByDayHelper {
   public:
    RollingByDayTestHelper() = default;

    void SetRollingTime(const TimePoint time) { time_to_roll_ = time; }

    TimePoint GetRollingTime() const { return time_to_roll_; }

    TimePoint CalcRollingTime(const TimePoint time, const int offset_seconds) {
        return CalcNextRollingTime(time, days::period::num, offset_seconds);
    }
};

TEST(RollingByDayHelperTest, NeedToRoll) {
    RollingByDayTestHelper        rolling{};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() + seconds{1}, .value_size{}};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingByDayHelperTest, NoNeedToRoll) {
    RollingByDayTestHelper        rolling{};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() - seconds{1}, .value_size{}};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingByDayHelperTest, Day_CalcNextRollingTime_BeforeOffset) {
    RollingByDayTestHelper rolling{};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + days{1} + seconds{kOffsetSeconds};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

TEST(RollingByDayHelperTest, Day_CalcNextRollingTime_AfterOffset) {
    RollingByDayTestHelper rolling{};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num + kOffsetSeconds;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + days{1};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

class RollingByHourTestHelper : public RollingByHourHelper {
   public:
    explicit RollingByHourTestHelper() = default;

    void SetRollingTime(const TimePoint time) { time_to_roll_ = time; }

    TimePoint GetRollingTime() const { return time_to_roll_; }

    TimePoint CalcRollingTime(const TimePoint time, const int offset_seconds) {
        return CalcNextRollingTime(time, hours::period::num, offset_seconds);
    }
};

TEST(RollingByHourTestHelper, NeedToRoll) {
    RollingByHourTestHelper       rolling{};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() + seconds{1}, .value_size{}};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingByHourTestHelper, NoNeedToRoll) {
    RollingByHourTestHelper       rolling{};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() - seconds{1}, .value_size{}};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingByHourTestHelper, Day_CalcNextRollingTime_BeforeOffset) {
    RollingByHourTestHelper rolling{};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + hours{1} + seconds{kOffsetSeconds};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

TEST(RollingByHourTestHelper, Day_CalcNextRollingTime_AfterOffset) {
    RollingByHourTestHelper rolling{};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num + hours::period::num + kOffsetSeconds;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + hours{1};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

class RollingBySizeTestHelper : public RollingBySizeHelper {
   public:
    explicit RollingBySizeTestHelper(const std::uint64_t size_limit_mb) : RollingBySizeHelper{size_limit_mb} {}

    void SetCurrentBytesize(const std::uint64_t bytesize) noexcept { current_bytesize_ = bytesize; }

    std::uint64_t GetCurrentBytesize() const noexcept { return current_bytesize_; }
};

TEST(RollingBySizeTestHelper, NeedToRoll_ValueSize) {
    RollingBySizeTestHelper rolling{100};

    const RollingHelper::Metadata metadata{.time{}, .value_size = 100 * std::mega::num};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingBySizeTestHelper, NeedToRoll_TotalSize) {
    RollingBySizeTestHelper rolling{100};

    constexpr std::uint64_t kInitBytesize = 99 * std::mega::num;
    rolling.SetCurrentBytesize(99 * std::mega::num);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    const RollingHelper::Metadata metadata{.time{}, .value_size = std::mega::num};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingBySizeTestHelper, NoNeedToRoll) {
    RollingBySizeTestHelper rolling{100};

    constexpr std::uint64_t kInitBytesize = 99 * std::mega::num;
    rolling.SetCurrentBytesize(kInitBytesize);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    const RollingHelper::Metadata metadata{.time{}, .value_size = std::mega::num / 2};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingBySizeTestHelper, Reset) {
    RollingBySizeTestHelper rolling{100};

    constexpr std::uint64_t kInitBytesize = 99 * std::mega::num;
    rolling.SetCurrentBytesize(kInitBytesize);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    rolling.Reset();
    EXPECT_EQ(0u, rolling.GetCurrentBytesize());
}

TEST(RollingBySizeTestHelper, Update) {
    RollingBySizeTestHelper rolling{100};

    constexpr std::uint64_t kInitBytesize = 90 * std::mega::num;
    rolling.SetCurrentBytesize(kInitBytesize);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    constexpr std::uint64_t kIncBytesize = 9 * std::mega::num;

    rolling.Update({.time{}, .value_size = kIncBytesize});
    EXPECT_EQ(kInitBytesize + kIncBytesize, rolling.GetCurrentBytesize());
}

}  // namespace cris::core
