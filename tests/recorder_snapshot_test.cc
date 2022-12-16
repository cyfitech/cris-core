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
    RecorderSnapshotTest() : testing::Test() { std::filesystem::create_directories(GetTestTempDir()); }
    ~RecorderSnapshotTest() { std::filesystem::remove_all(GetTestTempDir()); }

    std::filesystem::path GetTestTempDir() const { return record_test_temp_dir_; }

   private:
    std::filesystem::path record_test_temp_dir_{
        std::filesystem::temp_directory_path() / (std::string("CRSnapshotTestTmpDir.") + std::to_string(getpid()))};
};

template<class T>
    requires std::is_standard_layout_v<T>
struct TestMessage : public CRMessage<TestMessage<T>> {
    using data_type = T;

    TestMessage() = default;

    explicit TestMessage(T val) : value_(std::move(val)) {}

    T value_{};
};

template<class T>
void MessageFromStr(TestMessage<T>& msg, const std::string& serialized_msg) {
    std::memcpy(&msg.value_, serialized_msg.c_str(), std::min(sizeof(T), serialized_msg.size()));
}

template<class T>
std::string MessageToStr(const TestMessage<T>& msg) {
    std::string serialized_msg(sizeof(T), 0);
    std::memcpy(&serialized_msg[0], &msg.value_, serialized_msg.size());
    return serialized_msg;
}

TEST_F(RecorderSnapshotTest, RecorderSnapshotSingleIntervalTest) {
    static constexpr std::size_t     kThreadNum            = 4;
    static constexpr std::size_t     kMessageNum           = 40;
    static constexpr std::size_t     kFlakyIgnoreNum       = 1;
    static constexpr double          kSpeedUpFactor        = 1e9;
    static constexpr channel_subid_t kTestIntChannelSubId  = 11;
    static constexpr auto            kSleepBetweenMessages = std::chrono::milliseconds(100);

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner = JobRunner::MakeJobRunner(config);

    RecorderConfig::IntervalConfig interval_config{
        .name_         = std::string("SECONDLY"),
        .interval_sec_ = std::chrono::seconds(1),
    };

    RecorderConfig recorder_config{
        .snapshot_intervals_ = {interval_config},
        .record_dir_         = GetTestTempDir(),
    };

    MessageRecorder recorder(recorder_config, runner);
    recorder.RegisterChannel<TestMessage<std::size_t>>(kTestIntChannelSubId);
    core::CRNode publisher;

    auto wake_up_time = std::chrono::steady_clock::now();

    for (std::size_t i = 1; i <= kMessageNum; ++i) {
        auto test_message = std::make_shared<TestMessage<std::size_t>>(i);
        publisher.Publish(kTestIntChannelSubId, std::move(test_message));

        wake_up_time += kSleepBetweenMessages;
        std::this_thread::sleep_until(wake_up_time);
    }

    // Make sure messages arrive records
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Test content under each snapshot directory
    std::map<std::string, std::vector<std::filesystem::path>> snapshot_path_map = recorder.GetSnapshotPaths();

    for (const auto& path_pair : snapshot_path_map) {
        for (std::size_t entry_index = 0; entry_index < path_pair.second.size(); ++entry_index) {
            MessageReplayer replayer(path_pair.second[entry_index]);
            core::CRNode    subscriber(runner);

            replayer.RegisterChannel<TestMessage<std::size_t>>(kTestIntChannelSubId);
            replayer.SetSpeedupRate(kSpeedUpFactor);

            auto previous_value = std::make_shared<std::atomic<std::size_t>>(0);

            subscriber.Subscribe<TestMessage<std::size_t>>(
                kTestIntChannelSubId,
                [&previous_value](const std::shared_ptr<TestMessage<std::size_t>>& message) {
                    EXPECT_EQ(message->value_ - previous_value->load(), 1);
                    previous_value->store(message->value_);
                },
                /* allow_concurrency = */ false);

            replayer.MainLoop();

            // Make sure messages arrive the node
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Make sure the data ends around our snapshot timepoint
            // Example: if i = 4 when we made the snapshot, then we should have 01234
            if (entry_index == 0) {
                EXPECT_EQ(previous_value->load(), 0);
            } else {
                const std::size_t expect_value =
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(1)).count() /
                    kSleepBetweenMessages.count() * entry_index;
                EXPECT_TRUE(
                    (previous_value->load() >= expect_value - kFlakyIgnoreNum) &&
                    (previous_value->load() <= expect_value + kFlakyIgnoreNum));
            }
        }

        const std::size_t sleep_total_sec_count =
            std::chrono::duration_cast<std::chrono::seconds>(kMessageNum * kSleepBetweenMessages).count();

        // Plus the origin snapshot
        EXPECT_EQ(path_pair.second.size(), sleep_total_sec_count + 1);
    }
}
}  // namespace cris::core
