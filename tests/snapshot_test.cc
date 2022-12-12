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

class SnapshotTest : public testing::Test {
   public:
    SnapshotTest() : testing::Test() { std::filesystem::create_directories(GetTestTempDir()); }
    ~SnapshotTest() { std::filesystem::remove_all(GetTestTempDir()); }

    std::filesystem::path GetTestTempDir() const { return record_test_temp_dir_; }

    std::filesystem::path GetSnapshotTestTempDir() const { return snapshot_test_temp_dir_; }

   private:
    std::filesystem::path record_test_temp_dir_{
        std::filesystem::temp_directory_path() / (std::string("CRSnapshotTestTmpDir.") + std::to_string(getpid()))};
    std::filesystem::path snapshot_test_temp_dir_{
        record_test_temp_dir_ / std::string("Snapshot") / std::string("SECONDLY")};
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

TEST_F(SnapshotTest, SnapshotSingleIntervalTest) {
    static constexpr std::size_t     kThreadNum             = 4;
    static constexpr std::size_t     kMessageNum            = 4;
    static constexpr std::size_t     kMessagePerIntervalNum = 10;
    static constexpr double          kSpeedUpFactor         = 30.0f;
    static constexpr channel_subid_t kTestIntChannelSubId   = 11;

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
    recorder.RegisterChannel<TestMessage<int>>(kTestIntChannelSubId);
    core::CRNode publisher;

    // Origin snapshot
    std::vector<int> snapshot_point_list;
    snapshot_point_list.push_back(0);

    // Publish integer number 1-40 in 5 seconds
    auto wake_up_time = std::chrono::system_clock::now();

    for (std::size_t i = 1; i <= kMessageNum; ++i) {
        for (std::size_t j = 1; j <= kMessagePerIntervalNum; ++j) {
            auto test_message = std::make_shared<TestMessage<int>>(static_cast<int>((i - 1) * 10 + j));
            publisher.Publish(kTestIntChannelSubId, std::move(test_message));
        }
        snapshot_point_list.push_back(static_cast<int>(i * kMessagePerIntervalNum));

        wake_up_time += std::chrono::seconds(1);
        std::this_thread::sleep_until(wake_up_time);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Sort the path strings so that it matched with the snapshot_point_list values
    std::vector<std::filesystem::path> snapshot_path_list;
    for (const auto& dir_entry : std::filesystem::directory_iterator{GetSnapshotTestTempDir()}) {
        snapshot_path_list.push_back(dir_entry.path());
    }
    std::sort(snapshot_path_list.begin(), snapshot_path_list.end());

    // Test content under each snapshot directory
    std::size_t dir_entry_counter = 0;

    for (const auto& path : snapshot_path_list) {
        MessageReplayer replayer(path);
        auto            path_runner = JobRunner::MakeJobRunner(config);
        core::CRNode    subscriber(path_runner);

        replayer.RegisterChannel<TestMessage<int>>(kTestIntChannelSubId);
        replayer.SetSpeedupRate(kSpeedUpFactor);

        auto previous_int = std::make_shared<std::atomic<int>>(0);

        subscriber.Subscribe<TestMessage<int>>(
            kTestIntChannelSubId,
            [&previous_int](const std::shared_ptr<TestMessage<int>>& message) {
                // consecutive data without loss in snapshot
                EXPECT_EQ(message->value_ - previous_int->load(), 1);
                previous_int->store(message->value_);
            },
            /* allow_concurrency = */ false);

        replayer.MainLoop();

        // Make sure messages arrive the node
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Make sure the data ends at our snapshot timepoint
        // Example: if i = 4 when we made the snapshot, then we should have 01234
        EXPECT_EQ(previous_int->load(), snapshot_point_list[dir_entry_counter]);

        dir_entry_counter++;
        path_runner->Stop();
    }

    EXPECT_EQ(dir_entry_counter, snapshot_point_list.size());
}

}  // namespace cris::core
