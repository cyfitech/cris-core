#include "cris/core/config/recorder_config.h"
#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/recorder.h"
#include "cris/core/msg_recorder/replayer.h"
#include "cris/core/sched/job_runner.h"

#include "fmt/chrono.h"
#include "fmt/core.h"

#include "gtest/gtest.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <utility>

using std::filesystem::create_directories;
using std::filesystem::path;
using std::filesystem::remove_all;
using std::filesystem::temp_directory_path;

using channel_subid_t = cris::core::CRMessageBase::channel_subid_t;

static constexpr channel_subid_t kTestIntChannelSubId        = 11;
static constexpr channel_subid_t kTestSnapshotSingleInterval = 5;

namespace cris::core {

class RecorderPauseTest : public testing::Test {
   public:
    void SetUp() override { create_directories(GetRecordTestTempDir()); }

    void TearDown() override { remove_all(GetRecordTestTempDir()); }

    path GetRecordTestTempDir() { return record_test_temp_dir_; }

    RecorderConfigPtr GetTestConfig();

    void TestRecordFilePause();

    void TestRecordDataConsecutive();

   private:
    path record_test_temp_dir_{
        temp_directory_path() / (std::string("CRSnapshotTestTmpDir.") + std::to_string(getpid()))};
    path record_dir_;

    static constexpr std::size_t kThreadNum            = 4;
    static constexpr std::size_t kMessageNum           = 10;
    static constexpr auto        kSleepBetweenMessages = std::chrono::milliseconds(100);
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

RecorderConfigPtr RecorderPauseTest::GetTestConfig() {
    RecorderConfig temp_config;
    temp_config.snapshot_intervals_ = {std::chrono::duration<int>(kTestSnapshotSingleInterval)};
    temp_config.record_dir_         = GetRecordTestTempDir().string();
    return std::make_shared<RecorderConfig>(temp_config);
}

void RecorderPauseTest::TestRecordFilePause() {
    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto            runner = JobRunner::MakeJobRunner(config);
    MessageRecorder recorder(GetTestConfig(), runner);

    recorder.RegisterChannel<TestMessage<int>>(kTestIntChannelSubId);
    record_dir_ = recorder.GetRecordDir();

    core::CRNode publisher;

    for (std::size_t i = 0; i < kMessageNum; ++i) {
        if (i == 5) {
            recorder.MakeSnapshot();
        }
        auto test_message = std::make_shared<TestMessage<int>>(static_cast<int>(i));
        publisher.Publish(kTestIntChannelSubId, std::move(test_message));
        std::this_thread::sleep_for(kSleepBetweenMessages);
    }
    // Make sure messages arrive records
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void RecorderPauseTest::TestRecordDataConsecutive() {
    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto            runner = JobRunner::MakeJobRunner(config);
    MessageReplayer replayer(record_dir_);
    core::CRNode    subscriber(runner);

    replayer.RegisterChannel<TestMessage<int>>(kTestIntChannelSubId);

    int previous_int = -1;

    subscriber.Subscribe<TestMessage<int>>(
        kTestIntChannelSubId,
        [&previous_int](const std::shared_ptr<TestMessage<int>>& message) {
            // consecutive data without loss
            EXPECT_TRUE(message->value_ - previous_int == 1);
            previous_int = message->value_;
        },
        /* allow_concurrency = */ false);

    bool run_post_start  = false;
    bool run_pre_finish  = false;
    bool run_post_finish = false;
    replayer.SetPostStartCallback([&run_post_start, &replayer] {
        run_post_start = true;
        EXPECT_FALSE(replayer.IsEnded());
    });
    replayer.SetPreFinishCallback([&run_pre_finish, &replayer] {
        run_pre_finish = true;
        EXPECT_FALSE(replayer.IsEnded());
    });
    replayer.SetPostFinishCallback([&run_post_finish, &replayer] {
        run_post_finish = true;
        EXPECT_TRUE(replayer.IsEnded());
    });
    replayer.MainLoop();
    EXPECT_TRUE(run_post_start);
    EXPECT_TRUE(run_pre_finish);
    EXPECT_TRUE(run_post_finish);

    // Make sure messages arrive the node
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(RecorderPauseTest, RecorderPauseTest) {
    TestRecordFilePause();
    TestRecordDataConsecutive();
}

}  // namespace cris::core
