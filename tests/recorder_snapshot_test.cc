#include "cris/core/config/recorder_config.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/recorder.h"
#include "cris/core/msg_recorder/replayer.h"
#include "cris/core/sched/job_runner.h"

#include "fmt/chrono.h"
#include "fmt/core.h"

#include "gtest/gtest.h"

#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using channel_subid_t = cris::core::CRMessageBase::channel_subid_t;

namespace cris::core {

class RecorderSnapshotTest : public testing::Test {
   public:
    RecorderSnapshotTest() { std::filesystem::create_directories(GetTestTempDir()); }
    ~RecorderSnapshotTest() { std::filesystem::remove_all(GetTestTempDir()); }

    void TestSnapshot(RecorderConfig recorder_config);

    std::filesystem::path GetTestTempDir() const { return record_test_temp_dir_; }

   private:
    std::filesystem::path record_test_temp_dir_{
        std::filesystem::temp_directory_path() / (std::string("CRSnapshotTestTmpDir.") + std::to_string(getpid()))};
};

struct TestMessage : public CRMessage<TestMessage> {
    using data_type = std::size_t;

    TestMessage() = default;

    explicit TestMessage(std::size_t val) : value_(val) {}

    std::size_t value_{};
};

void MessageFromStr(TestMessage& msg, const std::string& serialized_msg) {
    std::memcpy(&msg.value_, serialized_msg.c_str(), std::min(sizeof(std::size_t), serialized_msg.size()));
}

std::string MessageToStr(const TestMessage& msg) {
    std::string serialized_msg(sizeof(std::size_t), 0);
    std::memcpy(&serialized_msg[0], &msg.value_, serialized_msg.size());
    return serialized_msg;
}

void RecorderSnapshotTest::TestSnapshot(RecorderConfig recorder_config) {
    std::mutex                       mtx;
    static constexpr std::size_t     kThreadNum            = 3;
    static constexpr std::size_t     kMessageNum           = 30;
    static constexpr channel_subid_t kTestIntChannelSubId  = 11;
    static constexpr auto            kSleepBetweenMessages = std::chrono::milliseconds(100);

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner = JobRunner::MakeJobRunner(config);

    MessageRecorder recorder(recorder_config, runner);
    recorder.RegisterChannel<TestMessage>(kTestIntChannelSubId);
    core::CRNode publisher;

    auto wake_up_time = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < kMessageNum; ++i) {
        auto test_message = std::make_shared<TestMessage>(i);
        publisher.Publish(kTestIntChannelSubId, std::move(test_message));

        wake_up_time += kSleepBetweenMessages;
        std::this_thread::sleep_until(wake_up_time);
    }

    // Test content under each snapshot directory
    std::map<std::string, std::vector<std::filesystem::path>> snapshot_path_map = recorder.GetSnapshotPaths();

    // Plus the origin snapshot
    const std::size_t kExpectedMinNum = kMessageNum * kSleepBetweenMessages / std::chrono::milliseconds(1) + 1;

    auto check_num_time  = std::chrono::steady_clock::now();
    // We may wait half of the interval time at max
    auto check_stop_time = check_num_time + std::chrono::milliseconds(500);

    while (snapshot_path_map["SECONDLY"].size() < kExpectedMinNum && check_num_time < check_stop_time) {
        check_num_time += kSleepBetweenMessages;
        std::this_thread::sleep_until(check_num_time);
        snapshot_path_map = recorder.GetSnapshotPaths();
    }

    // The message at the exact time point when the snapshot was token is allowed to be toleranted
    const std::size_t kFlakyTolerance = 1;

    EXPECT_EQ(snapshot_path_map.size(), recorder_config.snapshot_intervals_.size());

    for (const auto& path_pair : snapshot_path_map) {
        const std::size_t path_pair_index =
            static_cast<std::size_t>(distance(snapshot_path_map.begin(), snapshot_path_map.find(path_pair.first)));

        for (std::size_t entry_index = 0; entry_index < path_pair.second.size(); ++entry_index) {
            MessageReplayer replayer(path_pair.second[entry_index]);
            core::CRNode    subscriber(runner);

            replayer.RegisterChannel<TestMessage>(kTestIntChannelSubId);
            // Raise the replayer speed to erase any waiting time
            replayer.SetSpeedupRate(1e9);

            auto previous_size_t = std::make_shared<std::atomic<std::size_t>>(0);

            subscriber.Subscribe<TestMessage>(
                kTestIntChannelSubId,
                [&mtx, &previous_size_t](const std::shared_ptr<TestMessage>& message) {
                    std::lock_guard   lck(mtx);
                    if (message->value_ != 0) {
                        EXPECT_EQ(message->value_ - previous_size_t->load(), 1);
                    }
                    previous_size_t->store(message->value_);
                },
                /* allow_concurrency = */ false);

            replayer.MainLoop();

            // Make sure messages arrive the node
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Make sure the data ends around our snapshot timepoint
            // Example: if i = 4 when we made the snapshot, then we should have 01234
            {
                std::lock_guard   lck(mtx);
                const std::size_t previous_value = previous_size_t->load();

                if (entry_index == 0) {
                    EXPECT_EQ(previous_value, 0);
                } else {
                    const std::size_t expect_value =
                        static_cast<std::size_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                     recorder_config.snapshot_intervals_[path_pair_index].interval_sec_)
                                                     .count()) /
                        kSleepBetweenMessages.count() * entry_index;
                    EXPECT_TRUE(
                        (previous_value >= expect_value - kFlakyTolerance) &&
                        (previous_value <= expect_value + kFlakyTolerance));
                }
            }
        }

        EXPECT_EQ(path_pair.first, recorder_config.snapshot_intervals_[path_pair_index].name_);

        const std::size_t kTotalTimeSec =
            std::chrono::duration_cast<std::chrono::seconds>(kMessageNum * kSleepBetweenMessages).count();

        const std::size_t kCurrentIntervalSec =
            static_cast<std::size_t>(recorder_config.snapshot_intervals_[path_pair_index].interval_sec_.count());

        // Plus the origin snapshot
        const std::size_t kExpectedSnapshotNum = kTotalTimeSec / kCurrentIntervalSec + 1;

        EXPECT_EQ(path_pair.second.size(), kExpectedSnapshotNum);
    }
}

TEST_F(RecorderSnapshotTest, RecorderSnapshotSingleIntervalTest) {
    RecorderConfig::IntervalConfig interval_config{
        .name_         = std::string("SECONDLY"),
        .interval_sec_ = std::chrono::seconds(1),
    };

    RecorderConfig single_interval_config{
        .snapshot_intervals_ = {interval_config},
        .record_dir_         = GetTestTempDir(),
    };

    TestSnapshot(single_interval_config);
}

TEST_F(RecorderSnapshotTest, RecorderSnapshotMultiIntervalTest) {
    std::vector<RecorderConfig::IntervalConfig> interval_configs{
        {
            .name_         = std::string("SECONDLY"),
            .interval_sec_ = std::chrono::seconds(1),
        },
        {
            .name_         = std::string("SECONDS_3"),
            .interval_sec_ = std::chrono::seconds(3),
        },
    };

    RecorderConfig multi_interval_config{
        .snapshot_intervals_ = interval_configs,
        .record_dir_         = GetTestTempDir(),
    };

    TestSnapshot(multi_interval_config);
}

}  // namespace cris::core
