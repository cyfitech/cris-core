#include "cris/core/msg_recorder/recorder_config.h"

#include "cris/core/config/config.h"

#include <gtest/gtest.h>

#include <fmt/core.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

namespace cris::core {

namespace fs = std::filesystem;

class RecordConfigTest : public testing::Test {
   public:
    RecordConfigTest()
        : test_config_path_(fs::temp_directory_path() / fmt::format("record_config_test.pid.{}.json", getpid())) {}

    ~RecordConfigTest() override { fs::remove(test_config_path_); }

    ConfigFile MakeRecordConfigFile(std::string content) const {
        std::ofstream config_file(test_config_path_);
        config_file << content;
        config_file.flush();
        return ConfigFile(test_config_path_);
    }

    fs::path GetRecordConfigPath() const { return test_config_path_; };

   private:
    fs::path test_config_path_;
};

TEST_F(RecordConfigTest, RecorderConfigTestBasic) {
    {
        auto recorder_config_file = MakeRecordConfigFile(
            R"({
                "recorder": {
                    "hostname": "SOME_HOSTNAME",
                    "record_dir": "record_test"
                }
            })");

        RecorderConfig recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();

        EXPECT_EQ(recorder_config.snapshot_intervals_.size(), 0);
        EXPECT_EQ(recorder_config.record_dir_, "record_test");
    }

    {
        auto recorder_config_file = MakeRecordConfigFile(
            R"({
                "recorder": {
                    "hostname": "SOME_HOSTNAME",
                    "snapshot_intervals" : [],
                    "record_dir": "record_test"
                }
            })");

        RecorderConfig recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();

        EXPECT_EQ(recorder_config.snapshot_intervals_.size(), 0);
        EXPECT_EQ(recorder_config.record_dir_, "record_test");
    }

    {
        auto recorder_config_file = MakeRecordConfigFile(
            R"({
                "recorder": {
                    "hostname": "SOME_HOSTNAME",
                    "snapshot_intervals" : [
                        {
                            "name": "5 SECONDS",
                            "period_sec": 5,
                            "max_num_of_copies": 1
                        },
                        {
                            "name": "1 SECOND",
                            "period_sec": 1,
                            "max_num_of_copies": 5
                        }
                    ],
                    "record_dir": "record_test"
                }
            })");

        RecorderConfig recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();

        EXPECT_EQ(recorder_config.snapshot_intervals_.size(), 2);
        EXPECT_EQ(recorder_config.snapshot_intervals_.front().name_, "5 SECONDS");
        EXPECT_EQ(recorder_config.snapshot_intervals_.front().period_, std::chrono::seconds(5));
        EXPECT_EQ(recorder_config.snapshot_intervals_.front().max_num_of_copies_, 1);
        EXPECT_EQ(recorder_config.snapshot_intervals_.back().name_, "1 SECOND");
        EXPECT_EQ(recorder_config.snapshot_intervals_.back().period_, std::chrono::seconds(1));
        EXPECT_EQ(recorder_config.snapshot_intervals_.back().max_num_of_copies_, 5);
        EXPECT_EQ(recorder_config.record_dir_, "record_test");
    }

    {
        auto recorder_config_file = MakeRecordConfigFile(
            R"({
                "recorder": {
                    "hostname": "SOME_HOSTNAME",
                    "snapshot_intervals" : [
                        {
                            "name": "5 SECONDS",
                            "period_sec": 5
                        }
                    ]
                }
            })");

        RecorderConfig recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();

        EXPECT_EQ(recorder_config.snapshot_intervals_.size(), 1);
        EXPECT_EQ(recorder_config.snapshot_intervals_.front().name_, "5 SECONDS");
        EXPECT_EQ(recorder_config.snapshot_intervals_.front().period_, std::chrono::seconds(5));
        EXPECT_EQ(recorder_config.snapshot_intervals_.front().max_num_of_copies_, 48);
        EXPECT_EQ(recorder_config.record_dir_, "");
    }
}

TEST_F(RecordConfigTest, RecorderConfigTestInvalid) {
    {
        auto recorder_config_file = MakeRecordConfigFile(
            R"({
            "recorder": {
                "hostname": "SOME_HOSTNAME",
                "snapshot_intervals" : [
                    {
                        "name": "1 SECOND"
                    }
                ],
                "record_dir": "record_test"
            }
        })");

        // EXPECT_DEATH contains `goto`.
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
        EXPECT_DEATH(
            recorder_config_file.Get<RecorderConfig>("recorder"),
            R"("period_sec" is required. NO_SUCH_FIELD)");
    }

    {
        auto recorder_config_file = MakeRecordConfigFile(
            R"({
            "recorder": {
                "hostname": "SOME_HOSTNAME",
                "snapshot_intervals" : [],
                "record_dir": 5
            }
        })");

        // EXPECT_DEATH contains `goto`.
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
        EXPECT_DEATH(recorder_config_file.Get<RecorderConfig>("recorder"), "Expect a string for \"record_dir\"");
    }
}

TEST_F(RecordConfigTest, RecorderConfigTestDefault) {
    {
        auto recorder_config_file = MakeRecordConfigFile("{}");

        RecorderConfig recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();
        EXPECT_EQ(recorder_config.snapshot_intervals_.size(), 0);
        EXPECT_EQ(recorder_config.record_dir_, "");
        EXPECT_EQ(RecorderConfig::Rolling::kNone, recorder_config.rolling_);
    }
}

TEST_F(RecordConfigTest, RollingConfigByDayOK) {
    auto recorder_config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "hostname": "SOME_HOSTNAME",
                "rolling": "day"
            }
        })");
    const auto recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();
    EXPECT_EQ(RecorderConfig::Rolling::kDay, recorder_config.rolling_);
    EXPECT_EQ("SOME_HOSTNAME", recorder_config.hostname_);
}

TEST_F(RecordConfigTest, RollingConfigByHourOK) {
    auto recorder_config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "hostname": "SOME_HOSTNAME",
                "rolling": "hour"
            }
        })");
    const auto recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();
    EXPECT_EQ(RecorderConfig::Rolling::kHour, recorder_config.rolling_);
    EXPECT_EQ("SOME_HOSTNAME", recorder_config.hostname_);
}

TEST_F(RecordConfigTest, RollingConfigBySizeOK) {
    auto recorder_config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "hostname": "SOME_HOSTNAME",
                "rolling": "size",
                "size_limit_mb": 100
            }
        })");
    const auto recorder_config = recorder_config_file.Get<RecorderConfig>("recorder")->GetValue();
    EXPECT_EQ(RecorderConfig::Rolling::kSize, recorder_config.rolling_);
    EXPECT_EQ(100u, recorder_config.size_limit_mb_);
    EXPECT_EQ("SOME_HOSTNAME", recorder_config.hostname_);
}

TEST_F(RecordConfigTest, RollingConfigNonStringFail) {
    auto recorder_config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "rolling": 123
            }
        })");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(recorder_config_file.Get<RecorderConfig>("recorder"), R"(Expect a string for "rolling")");
}

TEST_F(RecordConfigTest, RollingConfigByUnknownFail) {
    auto recorder_config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "rolling": "unknown"
            }
        })");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(
        recorder_config_file.Get<RecorderConfig>("recorder"),
        R"(Expect a string for "rolling" be in \["day", "hour", "size"\])");
}

TEST_F(RecordConfigTest, RollingConfigBySizeInvalidSizeLimitTypeFail) {
    auto recorder_config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "rolling": "size",
                "size_limit_mb": "123"
            }
        })");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(
        recorder_config_file.Get<RecorderConfig>("recorder"),
        R"(Expect a non-zero positive integer for "size_limit_mb")");
}

TEST_F(RecordConfigTest, RollingConfigBySizeZeroSizeLimitFail) {
    auto recorder_config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "rolling": "size",
                "size_limit_mb": 0
            }
        })");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(
        recorder_config_file.Get<RecorderConfig>("recorder"),
        R"(Expect a non-zero positive integer for "size_limit_mb".)");
}

TEST_F(RecordConfigTest, NoHostnameFail) {
    auto config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "rolling": "day"
            }
        })");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(config_file.Get<RecorderConfig>("recorder"), R"(Expect a non-empty string for "hostname".)");
}

TEST_F(RecordConfigTest, EmptyHostnameFail) {
    auto config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "rolling": "hour",
                "hostname": ""
            }
        })");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(config_file.Get<RecorderConfig>("recorder"), R"(Expect a non-empty string for "hostname".)");
}

TEST_F(RecordConfigTest, NonStringHostnameFail) {
    auto config_file = MakeRecordConfigFile(
        R"({
            "recorder": {
                "rolling": "day",
                "hostname": 9527
            }
        })");
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-goto,hicpp-avoid-goto,-warnings-as-errors)
    EXPECT_DEATH(config_file.Get<RecorderConfig>("recorder"), R"(Expect a non-empty string for "hostname".)");
}

}  // namespace cris::core
