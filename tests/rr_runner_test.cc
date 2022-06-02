#include "cris/core/message/base.h"
#include "cris/core/node/multi_queue_node.h"
#include "cris/core/node/single_queue_node.h"
#include "cris/core/node_runner/runner.h"

#include "gtest/gtest.h"

#include <boost/functional/hash.hpp>

#include <cstring>
#include <unordered_map>

using namespace cris::core;

static constexpr std::size_t kMessageTypeNum = 7;
static constexpr std::size_t kMessageNum     = 100;
static constexpr std::size_t kMainThreadNum  = 4;

class CRSingleQueueNodeForTest : public CRSingleQueueNode<> {
   public:
    CRSingleQueueNodeForTest()
        : CRSingleQueueNode<>(
              kMessageTypeNum * kMessageNum,
              std::bind(&CRSingleQueueNodeForTest::QueueProcessor, this, std::placeholders::_1))
        , main_loopis_run_(kMainThreadNum, 0) {}

    void MainLoop(const std::size_t thread_idx, const std::size_t /* thread_num */) override {
        main_loopis_run_[thread_idx] = true;
    }

    bool IsMainLoopRun(std::size_t thread_idx) const { return main_loopis_run_[thread_idx]; }

   private:
    void SubscribeHandler(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback)
        override {
        subscriptions_.emplace(channel, std::move(callback));
    }

    void QueueProcessor(const CRMessageBasePtr& message) { return subscriptions_[message->GetChannelId()](message); }

    using callback_map_t =
        std::unordered_map<channel_id_t, std::function<void(const CRMessageBasePtr&)>, boost::hash<channel_id_t>>;

    std::vector<bool> main_loopis_run_;
    callback_map_t    subscriptions_;
};

class CRMultiQueueNodeForTest : public CRMultiQueueNode<> {
   public:
    CRMultiQueueNodeForTest() : CRMultiQueueNode<>(kMessageNum), main_loopis_run_(kMainThreadNum, 0) {}

    void MainLoop(const std::size_t thread_idx, const std::size_t /* thread_num */) {
        main_loopis_run_[thread_idx] = true;
    }

    bool IsMainLoopRun(std::size_t thread_idx) const { return main_loopis_run_[thread_idx]; }

   private:
    std::vector<bool> main_loopis_run_;
};

class RRRunnerTest : public testing::Test {
   public:
    void SetUp() override { std::memset(count_, 0, sizeof(count_)); }

    template<int idx>
    struct MessageForTest : public CRMessage<MessageForTest<idx>> {
        MessageForTest(int value, RRRunnerTest* test) : value_(value), test_(test) {}

        void Process() { ++test_->count_[idx][value_]; }

        int           value_;
        RRRunnerTest* test_;
    };

    template<int idx>
    static void MessageProcessorForTest(const std::shared_ptr<MessageForTest<idx>>& message) {
        message->Process();
    }

    void TestImpl(CRNodeBase* node, CRNodeRunnerBase* node_runner);

    unsigned count_[kMessageTypeNum][kMessageNum];
};

TEST_F(RRRunnerTest, SingleQueueSingleThread) {
    static constexpr auto    kQueueProcessorThreadNum = 1;
    CRSingleQueueNodeForTest node;
    CRNodeRoundRobinRunner   node_runner(&node, kQueueProcessorThreadNum, kMainThreadNum);
    TestImpl(&node, &node_runner);

    for (std::size_t i = 0; i < kMainThreadNum; ++i) {
        EXPECT_TRUE(node.IsMainLoopRun(i));
    }
}

TEST_F(RRRunnerTest, SingleQueueMultiThread) {
    static constexpr auto    kQueueProcessorThreadNum = 3;
    CRSingleQueueNodeForTest node;
    CRNodeRoundRobinRunner   node_runner(&node, kQueueProcessorThreadNum, kMainThreadNum);
    TestImpl(&node, &node_runner);

    for (std::size_t i = 0; i < kMainThreadNum; ++i) {
        EXPECT_TRUE(node.IsMainLoopRun(i));
    }
}

TEST_F(RRRunnerTest, MultiQueueSingleThread) {
    static constexpr auto   kQueueProcessorThreadNum = 1;
    CRMultiQueueNodeForTest node;
    CRNodeRoundRobinRunner  node_runner(&node, kQueueProcessorThreadNum, kMainThreadNum);
    TestImpl(&node, &node_runner);

    for (std::size_t i = 0; i < kMainThreadNum; ++i) {
        EXPECT_TRUE(node.IsMainLoopRun(i));
    }
}

TEST_F(RRRunnerTest, MultiQueueMultiThread) {
    static constexpr auto   kQueueProcessorThreadNum = 3;
    CRMultiQueueNodeForTest node;
    CRNodeRoundRobinRunner  node_runner(&node, kQueueProcessorThreadNum, kMainThreadNum);
    TestImpl(&node, &node_runner);

    for (std::size_t i = 0; i < kMainThreadNum; ++i) {
        EXPECT_TRUE(node.IsMainLoopRun(i));
    }
}

void RRRunnerTest::TestImpl(CRNodeBase* node, CRNodeRunnerBase* node_runner) {
    constexpr CRMessageBase::channel_subid_t kChannelSubIDForTest = 1;
    CRMultiQueueNode                         publisher(0);

    for (std::size_t i = 0; i < kMessageTypeNum; ++i) {
        for (std::size_t j = 0; j < kMessageNum; ++j) {
            EXPECT_EQ(count_[i][j], 0);
        }
    }

    node->Subscribe<MessageForTest<0>>(kChannelSubIDForTest, &RRRunnerTest::MessageProcessorForTest<0>);
    node->Subscribe<MessageForTest<1>>(kChannelSubIDForTest, &RRRunnerTest::MessageProcessorForTest<1>);
    node->Subscribe<MessageForTest<2>>(kChannelSubIDForTest, &RRRunnerTest::MessageProcessorForTest<2>);
    node->Subscribe<MessageForTest<3>>(kChannelSubIDForTest, &RRRunnerTest::MessageProcessorForTest<3>);
    node->Subscribe<MessageForTest<4>>(kChannelSubIDForTest, &RRRunnerTest::MessageProcessorForTest<4>);
    node->Subscribe<MessageForTest<5>>(kChannelSubIDForTest, &RRRunnerTest::MessageProcessorForTest<5>);
    node->Subscribe<MessageForTest<6>>(kChannelSubIDForTest, &RRRunnerTest::MessageProcessorForTest<6>);
    node_runner->Run();

    for (std::size_t i = 0; i < kMessageNum; ++i) {
        publisher.Publish(kChannelSubIDForTest, std::make_shared<MessageForTest<0>>(i, this));
        publisher.Publish(kChannelSubIDForTest, std::make_shared<MessageForTest<1>>(i, this));
        publisher.Publish(kChannelSubIDForTest, std::make_shared<MessageForTest<2>>(i, this));
        publisher.Publish(kChannelSubIDForTest, std::make_shared<MessageForTest<3>>(i, this));
        publisher.Publish(kChannelSubIDForTest, std::make_shared<MessageForTest<4>>(i, this));
        publisher.Publish(kChannelSubIDForTest, std::make_shared<MessageForTest<5>>(i, this));
        publisher.Publish(kChannelSubIDForTest, std::make_shared<MessageForTest<6>>(i, this));
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (std::size_t i = 0; i < kMessageTypeNum; ++i) {
        for (std::size_t j = 0; j < kMessageNum; ++j) {
            EXPECT_EQ(count_[i][j], 1);
        }
    }
}
