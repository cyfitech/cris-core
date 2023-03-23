#include "cris/core/msg_recorder/rolling_helper.h"

#include <gtest/gtest.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>

using namespace boost::posix_time;

namespace cris::core {

static const RollingHelper::RecordDirPathGenerator dir_path_generator = [] {
    return "record-dir";
};

class DummyRollingHelper : public RollingHelper {
   public:
    explicit DummyRollingHelper(const RecordDirPathGenerator* dirpath_generator) : RollingHelper{dirpath_generator} {}

    bool NeedToRoll([[maybe_unused]] const Metadata& metadata) const override { return false; }

    void Update([[maybe_unused]] const Metadata& metadata) override {}
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

    void SetTime(const ptime time) { last_write_time_ = time; }

    ptime GetTime() const { return last_write_time_; }
};

TEST(RollingByDayHelperTest, NeedToRoll) {
    RollingByDayTestHelper        rolling{&dir_path_generator};
    const RollingHelper::Metadata metadata{.time = rolling.GetTime() + hours{24}, .value_size{}};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingByDayHelperTest, NoNeedToRoll_PreviousTimestamp) {
    RollingByDayTestHelper        rolling{&dir_path_generator};
    const RollingHelper::Metadata metadata{.time = rolling.GetTime() - seconds{1}, .value_size{}};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingByDayHelperTest, NoNeedToRoll_InSameUtcDay) {
    RollingByDayTestHelper rolling{&dir_path_generator};
    rolling.SetTime(from_iso_string("20020131T000000"));

    const RollingHelper::Metadata metadata{.time = rolling.GetTime() + seconds{1000}, .value_size{}};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
}

TEST(RollingByDayHelperTest, Update) {
    RollingByDayTestHelper rolling{&dir_path_generator};
    rolling.SetTime(from_iso_string("20020131T000000"));

    const RollingHelper::Metadata metadata{.time = rolling.GetTime() + seconds{10}, .value_size{}};

    rolling.Update(metadata);
    EXPECT_EQ(rolling.GetTime(), metadata.time);
}

TEST(RollingByDayHelperTest, NoUpdate_PreviousTime) {
    RollingByDayTestHelper rolling{&dir_path_generator};
    const auto             now{second_clock::universal_time()};
    rolling.SetTime(now);

    const RollingHelper::Metadata metadata{.time = rolling.GetTime() - seconds{1}, .value_size{}};

    rolling.Update(metadata);
    EXPECT_NE(rolling.GetTime(), metadata.time);
    EXPECT_EQ(rolling.GetTime(), now);
}

class RollingByHourTestHelper : public RollingByHourHelper {
   public:
    explicit RollingByHourTestHelper(const RecordDirPathGenerator* dirpath_generator)
        : RollingByHourHelper{dirpath_generator} {}

    void SetTime(const ptime time) { last_write_time_ = time; }

    ptime GetTime() const { return last_write_time_; }
};

TEST(RollingByHourTestHelper, NeedToRoll) {
    RollingByHourTestHelper rolling{&dir_path_generator};
    const auto              now = second_clock::universal_time();
    rolling.SetTime(now - hours{1});

    const RollingHelper::Metadata metadata{.time = now, .value_size{}};
    EXPECT_TRUE(rolling.NeedToRoll(metadata));
}

TEST(RollingByHourTestHelper, NoNeedToRoll_InSameUtcHour) {
    RollingByHourTestHelper rolling{&dir_path_generator};
    rolling.SetTime(from_iso_string("20020131T000000"));

    const RollingHelper::Metadata metadata{.time = rolling.GetTime() + seconds{100}, .value_size{}};
    EXPECT_FALSE(rolling.NeedToRoll(metadata));
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

TEST(TimeTest, IsNotSameDay) {
    const auto start = boost::posix_time::from_iso_string("20020131T235959");
    const auto end   = boost::posix_time::from_iso_string("20020201T000000");
    EXPECT_FALSE(SameDay(start, end));
}

TEST(TimeTest, IsSameDay) {
    const auto start = boost::posix_time::from_iso_string("20020131T000000");
    const auto end   = boost::posix_time::from_iso_string("20020131T235959");
    EXPECT_TRUE(SameDay(start, end));
}

TEST(TimeTest, IsNotSameHour) {
    const auto start = boost::posix_time::from_iso_string("20020131T225959");
    const auto end   = boost::posix_time::from_iso_string("20020131T235959");
    EXPECT_FALSE(SameHour(start, end));
}

TEST(TimeTest, IsSameHour) {
    const auto start = boost::posix_time::from_iso_string("20020131T230000");
    const auto end   = boost::posix_time::from_iso_string("20020131T235959");
    EXPECT_TRUE(SameHour(start, end));
}

}  // namespace cris::core
