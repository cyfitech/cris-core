#include "cris/core/msg/node.h"
#include "cris/core/msg_recorder/recorder.h"
#include "cris/core/msg_recorder/replayer.h"
#include "cris/core/sched/job_runner.h"

#include "gtest/gtest.h"

#include <atomic>
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

static constexpr channel_subid_t kTestIntChannelSubId    = 11;
static constexpr channel_subid_t kTestDoubleChannelSubId = 12;

namespace cris::core {

class RecorderTest : public testing::Test {
   public:
    void SetUp() override { create_directories(GetTestTempDir()); }

    void TearDown() override { remove_all(GetTestTempDir()); }

    path GetTestTempDir() { return test_temp_dir_; }

    void TestRecord();

    void TestReplay(double speed_up);

    void TestReplayCanceled();

   private:
    path test_temp_dir_{temp_directory_path() / (std::string("CRTestTmpDir.") + std::to_string(getpid()))};
    path record_dir_;

    static constexpr int         kMessageManagerThreadNum = 1;
    static constexpr std::size_t kMessageNum              = 10;
    static constexpr auto        kSleepBetweenMessages    = std::chrono::milliseconds(100);
    static constexpr auto        kTotalRecordTime         = (kMessageNum - 1) * kSleepBetweenMessages;
};

#define CR_EXPECT_NEAR_DURATION(val1, val2, error)                                        \
    do {                                                                                  \
        using namespace std::chrono;                                                      \
        const auto lower_ns = duration_cast<nanoseconds>((1 - (error)) * (val2)).count(); \
        const auto upper_ns = duration_cast<nanoseconds>((1 + (error)) * (val2)).count(); \
        const auto val1_ns  = duration_cast<nanoseconds>(val1).count();                   \
        EXPECT_LT(lower_ns, val1_ns);                                                     \
        EXPECT_LT(val1_ns, upper_ns);                                                     \
    } while (0)

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

void RecorderTest::TestRecord() {
    auto            runner = core::JobRunner::MakeJobRunner({});
    MessageRecorder recorder(GetTestTempDir(), 1, runner);

    recorder.RegisterChannel<TestMessage<int>>(kTestIntChannelSubId);
    recorder.RegisterChannel<TestMessage<double>>(kTestDoubleChannelSubId);
    record_dir_ = recorder.GetRecordDir();

    core::CRNode publisher;

    for (std::size_t i = 0; i < kMessageNum; ++i) {
        if (i % 2 == 0) {
            auto test_message = std::make_shared<TestMessage<int>>(static_cast<int>(i));
            publisher.Publish(kTestIntChannelSubId, std::move(test_message));
        } else {
            auto test_message = std::make_shared<TestMessage<double>>(static_cast<double>(i));
            publisher.Publish(kTestDoubleChannelSubId, std::move(test_message));
        }
        std::this_thread::sleep_for(kSleepBetweenMessages);
    }

    // Make sure messages arrive records
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void RecorderTest::TestReplay(double speed_up) {
    auto            runner = core::JobRunner::MakeJobRunner({});
    MessageReplayer replayer(record_dir_);
    core::CRNode    subscriber(runner);

    replayer.RegisterChannel<TestMessage<int>>(kTestIntChannelSubId);
    replayer.RegisterChannel<TestMessage<double>>(kTestDoubleChannelSubId);
    replayer.SetSpeedupRate(speed_up);

    auto int_msg_count    = std::make_shared<std::atomic<int>>(0);
    auto double_msg_count = std::make_shared<std::atomic<int>>(0);

    std::chrono::steady_clock::time_point first_message_timestamp{};

    subscriber.Subscribe<TestMessage<int>>(
        kTestIntChannelSubId,
        [int_msg_count, speed_up, &first_message_timestamp](const std::shared_ptr<TestMessage<int>>& message) {
            auto current = int_msg_count->fetch_add(1);
            auto value   = message->value_;
            EXPECT_TRUE(value % 2 == 0);
            EXPECT_EQ(value / 2, current);
            if (value == 0) {
                first_message_timestamp = std::chrono::steady_clock::now();
            } else {
                auto elasped = std::chrono::steady_clock::now() - first_message_timestamp;
                CR_EXPECT_NEAR_DURATION(elasped * speed_up, value * kSleepBetweenMessages, 0.3);
            }
        },
        /* allow_concurrency = */ false);
    subscriber.Subscribe<TestMessage<double>>(
        kTestDoubleChannelSubId,
        [double_msg_count, speed_up, &first_message_timestamp](const std::shared_ptr<TestMessage<double>>& message) {
            auto current   = double_msg_count->fetch_add(1);
            auto int_value = static_cast<int>(message->value_);
            EXPECT_FALSE(int_value % 2 == 0);
            EXPECT_EQ(int_value / 2, current);
            auto elasped = std::chrono::steady_clock::now() - first_message_timestamp;
            CR_EXPECT_NEAR_DURATION(elasped * speed_up, int_value * kSleepBetweenMessages, 0.3);
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
    auto replayer_start = std::chrono::steady_clock::now();
    replayer.MainLoop();
    auto replayer_end      = std::chrono::steady_clock::now();
    auto replayer_duration = replayer_end - replayer_start;
    EXPECT_TRUE(run_post_start);
    EXPECT_TRUE(run_pre_finish);
    EXPECT_TRUE(run_post_finish);

    // Make sure messages arrive the node
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(int_msg_count->load(), kMessageNum / 2);
    EXPECT_EQ(double_msg_count->load(), kMessageNum / 2);
    CR_EXPECT_NEAR_DURATION(replayer_duration * speed_up, kTotalRecordTime, 0.3);
}

void RecorderTest::TestReplayCanceled() {
    MessageReplayer replayer(record_dir_);

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

    replayer.RegisterChannel<TestMessage<int>>(kTestIntChannelSubId);

    std::thread main_loop_thread([&replayer] { replayer.MainLoop(); });
    std::this_thread::sleep_for(kTotalRecordTime / 10);
    replayer.StopMainLoop();
    main_loop_thread.join();

    EXPECT_TRUE(run_post_start);
    EXPECT_TRUE(run_pre_finish);
    EXPECT_TRUE(run_post_finish);
}

TEST_F(RecorderTest, RecorderTest) {
    TestRecord();
    TestReplay(1.0);
    TestReplay(2.0);
    TestReplay(0.5);
    TestReplayCanceled();
}

}  // namespace cris::core
