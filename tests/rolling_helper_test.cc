#include "cris/core/msg_recorder/rolling_helper.h"

#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <memory>
#include <stdexcept>

using namespace std::chrono;

namespace cris::core {

using TimePoint = RollingHelper::Metadata::TimePoint;

static const RollingHelper::RecordDirPathGenerator dir_path_generator = [] {
    return "record-dir";
};

class DummyRollingHelper : public RollingHelper {
   public:
    explicit DummyRollingHelper(const RecordDirPathGenerator* dirpath_generator) : RollingHelper{dirpath_generator} {}

    bool NeedToRoll(const Metadata&) const override { return false; }

    void Update(const Metadata&) override {}

    void Reset() override {}
};

TEST(RollingHelperTest, NullDirPathGenerator_Crash) {
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_THROW(DummyRollingHelper{nullptr}, std::logic_error);
}

TEST(RollingHelperTest, NonCallableDirPathGenerator_Crash) {
    const RollingHelper::RecordDirPathGenerator invalid_dir_path_generator;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_THROW(DummyRollingHelper{&invalid_dir_path_generator}, std::logic_error);
}

TEST(RollingHelperTest, InitOK) {
    EXPECT_NE(nullptr, std::make_unique<DummyRollingHelper>(&dir_path_generator));
}

class RollingByDayTestHelper : public RollingByDayHelper {
   public:
    explicit RollingByDayTestHelper(const RecordDirPathGenerator* dirpath_generator)
        : RollingByDayHelper{dirpath_generator} {}

    void SetRollingTime(const TimePoint time) { time_to_roll_ = time; }

    TimePoint GetRollingTime() const { return time_to_roll_; }

    TimePoint CalcRollingTime(const TimePoint time, const int offset_seconds) {
        return CalcNextRollingTime(time, days::period::num, offset_seconds);
    }
};

TEST(RollingByDayHelperTest, NeedToRoll) {
    RollingByDayTestHelper        rolling{&dir_path_generator};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() + seconds{1}, .value_size{}};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingByDayHelperTest, NoNeedToRoll) {
    RollingByDayTestHelper        rolling{&dir_path_generator};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() - seconds{1}, .value_size{}};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingByDayHelperTest, Day_CalcNextRollingTime_BeforeOffset) {
    RollingByDayTestHelper rolling{&dir_path_generator};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + days{1} + seconds{kOffsetSeconds};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

TEST(RollingByDayHelperTest, Day_CalcNextRollingTime_AfterOffset) {
    RollingByDayTestHelper rolling{&dir_path_generator};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num + kOffsetSeconds;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + days{1};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

class RollingByHourTestHelper : public RollingByHourHelper {
   public:
    explicit RollingByHourTestHelper(const RecordDirPathGenerator* dirpath_generator) noexcept
        : RollingByHourHelper{dirpath_generator} {}

    void SetRollingTime(const TimePoint time) { time_to_roll_ = time; }

    TimePoint GetRollingTime() const { return time_to_roll_; }

    TimePoint CalcRollingTime(const TimePoint time, const int offset_seconds) {
        return CalcNextRollingTime(time, hours::period::num, offset_seconds);
    }
};

TEST(RollingByHourTestHelper, NeedToRoll) {
    RollingByHourTestHelper       rolling{&dir_path_generator};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() + seconds{1}, .value_size{}};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingByHourTestHelper, NoNeedToRoll) {
    RollingByHourTestHelper       rolling{&dir_path_generator};
    const RollingHelper::Metadata metadata{.time = rolling.GetRollingTime() - seconds{1}, .value_size{}};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingByHourTestHelper, Day_CalcNextRollingTime_BeforeOffset) {
    RollingByHourTestHelper rolling{&dir_path_generator};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + hours{1} + seconds{kOffsetSeconds};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

TEST(RollingByHourTestHelper, Day_CalcNextRollingTime_AfterOffset) {
    RollingByHourTestHelper rolling{&dir_path_generator};

    constexpr int     kOffsetSeconds        = 60;
    const std::time_t unix_time             = 100 * days::period::num + hours::period::num + kOffsetSeconds;
    const auto        time                  = system_clock::from_time_t(unix_time);
    const auto        time_to_roll          = rolling.CalcRollingTime(time, kOffsetSeconds);
    const auto        expected_rolling_time = time + hours{1};
    EXPECT_EQ(expected_rolling_time, time_to_roll);
}

class RollingBySizeTestHelper : public RollingBySizeHelper {
   public:
    explicit RollingBySizeTestHelper(
        const RecordDirPathGenerator* dirpath_generator,
        const std::uint64_t           size_limit_mb) noexcept
        : RollingBySizeHelper{dirpath_generator, size_limit_mb} {}

    void SetCurrentBytesize(const std::uint64_t bytesize) noexcept { current_bytesize_ = bytesize; }

    std::uint64_t GetCurrentBytesize() const noexcept { return current_bytesize_; }
};

TEST(RollingBySizeTestHelper, NeedToRoll_ValueSize) {
    RollingBySizeTestHelper rolling{&dir_path_generator, 100};

    const RollingHelper::Metadata metadata{.time{}, .value_size = 100 * std::mega::num};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingBySizeTestHelper, NeedToRoll_TotalSize) {
    RollingBySizeTestHelper rolling{&dir_path_generator, 100};

    constexpr std::uint64_t kInitBytesize = 99 * std::mega::num;
    rolling.SetCurrentBytesize(99 * std::mega::num);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    const RollingHelper::Metadata metadata{.time{}, .value_size = std::mega::num};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingBySizeTestHelper, NoNeedToRoll) {
    RollingBySizeTestHelper rolling{&dir_path_generator, 100};

    constexpr std::uint64_t kInitBytesize = 99 * std::mega::num;
    rolling.SetCurrentBytesize(kInitBytesize);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    const RollingHelper::Metadata metadata{.time{}, .value_size = std::mega::num / 2};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingBySizeTestHelper, Reset) {
    RollingBySizeTestHelper rolling{&dir_path_generator, 100};

    constexpr std::uint64_t kInitBytesize = 99 * std::mega::num;
    rolling.SetCurrentBytesize(kInitBytesize);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    rolling.Reset();
    EXPECT_EQ(0u, rolling.GetCurrentBytesize());
}

TEST(RollingBySizeTestHelper, Update) {
    RollingBySizeTestHelper rolling{&dir_path_generator, 100};

    constexpr std::uint64_t kInitBytesize = 90 * std::mega::num;
    rolling.SetCurrentBytesize(kInitBytesize);
    EXPECT_EQ(kInitBytesize, rolling.GetCurrentBytesize());

    constexpr std::uint64_t kIncBytesize = 9 * std::mega::num;

    rolling.Update({.time{}, .value_size = kIncBytesize});
    EXPECT_EQ(kInitBytesize + kIncBytesize, rolling.GetCurrentBytesize());
}

}  // namespace cris::core
