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
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
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

TEST_F(RecorderSnapshotTest, RecorderSnapshotTest) {
    static constexpr std::size_t     kThreadNum            = 4;
    static constexpr std::size_t     kMessageNum           = 40;
    static constexpr channel_subid_t kTestIntChannelSubId  = 11;
    static constexpr auto            kSleepBetweenMessages = std::chrono::milliseconds(100);

    RecorderConfig recorder_config{
        .snapshot_intervals_ =
            std::vector<RecorderConfig::IntervalConfig>{
                {
                    .name_   = std::string("1 SECOND"),
                    .period_ = std::chrono::seconds(1),
                },
                {
                    .name_   = std::string("2 SECONDS"),
                    .period_ = std::chrono::seconds(2),
                },
                {
                    .name_   = std::string("4 SECONDS"),
                    .period_ = std::chrono::seconds(4),
                },
            },
        .record_dir_ = GetTestTempDir(),
    };

    auto runner = JobRunner::MakeJobRunner(JobRunner::Config{
        .thread_num_ = kThreadNum,
    });

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

    static constexpr auto kMaxWaitingTime = std::chrono::milliseconds(500);

    auto check_expected_nums = [&recorder_config, &snapshot_path_map]() -> bool {
        for (const RecorderConfig::IntervalConfig& interval_config : recorder_config.snapshot_intervals_) {
            // Plus the origin snapshot
            const std::size_t kCurrentIntervalExpectedNum =
                kMessageNum * kSleepBetweenMessages / interval_config.period_ + 1;

            if (snapshot_path_map[interval_config.name_].size() < kCurrentIntervalExpectedNum) {
                return false;
            }
        }
        return true;
    };

    auto check_num_time  = std::chrono::steady_clock::now();
    // We may wait half of the interval time at max
    auto check_stop_time = check_num_time + kMaxWaitingTime;

    // Wait for the snapshot to be finished at the last second
    while (!check_expected_nums() && check_num_time < check_stop_time) {
        check_num_time += kSleepBetweenMessages;
        std::this_thread::sleep_until(check_num_time);
        snapshot_path_map = recorder.GetSnapshotPaths();
    }

    // The message at the exact time point when the snapshot was token is allowed to be toleranted
    constexpr std::size_t kFlakyTolerance = 2;

    EXPECT_EQ(snapshot_path_map.size(), recorder_config.snapshot_intervals_.size());

    for (const auto& snapshot_interval : recorder_config.snapshot_intervals_) {
        auto search = snapshot_path_map.find(snapshot_interval.name_);
        EXPECT_TRUE(search != snapshot_path_map.end());
        auto snapshot_dirs = search->second;

        for (std::size_t snapshot_dir_index = 0; snapshot_dir_index < snapshot_dirs.size(); ++snapshot_dir_index) {
            MessageReplayer replayer(snapshot_dirs[snapshot_dir_index]);
            core::CRNode    subscriber(runner);

            replayer.RegisterChannel<TestMessage>(kTestIntChannelSubId);

            // Raise the replayer speed to erase any waiting time
            replayer.SetSpeedupRate(1e9);

            std::mutex              replay_complete_mtx;
            std::condition_variable replay_complete_cv;

            auto                  previous_value_sp   = std::make_shared<std::size_t>(0);
            bool                  replay_is_complete  = false;
            constexpr std::size_t kLastMessageFlagNum = 989898;

            subscriber.Subscribe<TestMessage>(
                kTestIntChannelSubId,
                [previous_value_sp, &replay_is_complete, &replay_complete_cv, &replay_complete_mtx](
                    const std::shared_ptr<TestMessage>& message) {
                    if (message->value_ == kLastMessageFlagNum) {
                        std::lock_guard lck(replay_complete_mtx);
                        replay_is_complete = true;
                        replay_complete_cv.notify_all();
                        return;
                    }
                    if (message->value_ != 0) {
                        EXPECT_EQ(message->value_ - *previous_value_sp, 1);
                    }
                    *previous_value_sp = message->value_;
                },
                /* allow_concurrency = */ false);

            replayer.MainLoop();
            publisher.Publish(kTestIntChannelSubId, std::make_shared<TestMessage>(kLastMessageFlagNum));

            std::unique_lock lock(replay_complete_mtx);
            replay_complete_cv.wait(lock, [&replay_is_complete] { return replay_is_complete; });

            // Make sure the data ends around our snapshot timepoint
            // Example: if i = 4 when we made the snapshot, then we should have 01234
            const std::size_t last_recorded = *previous_value_sp;

            if (snapshot_dir_index == 0) {
                EXPECT_LE(last_recorded, 0 + kFlakyTolerance);
            } else {
                const std::size_t expect_num_end_with =
                    snapshot_interval.period_ * snapshot_dir_index / kSleepBetweenMessages;

                EXPECT_GE(last_recorded, expect_num_end_with - kFlakyTolerance);
                EXPECT_LE(last_recorded, expect_num_end_with + kFlakyTolerance);
            }
        }

        // Plus the origin snapshot
        const std::size_t kExpectedSnapshotNum = kMessageNum * kSleepBetweenMessages / snapshot_interval.period_ + 1;
        EXPECT_EQ(snapshot_dirs.size(), kExpectedSnapshotNum);
    }

    runner->Stop();
}

}  // namespace cris::core
