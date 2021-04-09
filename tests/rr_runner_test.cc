#include <cstring>

#include "gtest/gtest.h"
#include "node/multi_queue_node.h"
#include "node/runner.h"
#include "node/single_queue_node.h"

using namespace cris::core;

class RRRunnerTest : public testing::Test {
   public:
    void SetUp() override { std::memset(mCount, 0, sizeof(mCount)); }

    template<int idx>
    struct MessageForTest : public CRMessage<MessageForTest<idx>> {
        MessageForTest(int value, RRRunnerTest* test) : mValue(value), mTest(test) {}

        void Process() { ++mTest->mCount[idx][mValue]; }

        int           mValue;
        RRRunnerTest* mTest;
    };

    template<int idx>
    static void MessageProcessorForTest(const CRMessageBasePtr& message) {
        reinterpret_cast<MessageForTest<idx>*>(message.get())->Process();
    }

    class CRSingleQueueNodeForTest : public CRSingleQueueNode {
       public:
        CRSingleQueueNodeForTest()
            : CRSingleQueueNode(kMessageTypeNum * kMessageNum,
                                std::bind(&CRSingleQueueNodeForTest::QueueProcessor,
                                          this,
                                          std::placeholders::_1)) {}

       private:
        void SubscribeHandler(std::string&&                                  message_name,
                              std::function<void(const CRMessageBasePtr&)>&& callback) override {
            mSubscriptions.emplace(std::move(message_name), std::move(callback));
        }

        void QueueProcessor(const CRMessageBasePtr& message) {
            return mSubscriptions[message->GetMessageTypeName()](message);
        }

        std::map<std::string, std::function<void(const CRMessageBasePtr&)>> mSubscriptions;
    };

    void TestImpl(CRNodeBase* node, CRNodeRunner* node_runner);

    static constexpr size_t kMessageTypeNum = 7;
    static constexpr size_t kMessageNum     = 100;

    unsigned mCount[kMessageTypeNum][kMessageNum];
};

TEST_F(RRRunnerTest, SingleQueueSingleThread) {
    static constexpr auto    kRunnerThreadNum = 1;
    CRSingleQueueNodeForTest node;
    CRNodeRoundRobinRunner   node_runner(&node, kRunnerThreadNum);
    TestImpl(&node, &node_runner);
}

TEST_F(RRRunnerTest, SingleQueueMultiThread) {
    static constexpr auto    kRunnerThreadNum = 3;
    CRSingleQueueNodeForTest node;
    CRNodeRoundRobinRunner   node_runner(&node, kRunnerThreadNum);
    TestImpl(&node, &node_runner);
}

TEST_F(RRRunnerTest, MultiQueueSingleThread) {
    static constexpr auto  kRunnerThreadNum = 1;
    CRMultiQueueNode       node(kMessageNum);
    CRNodeRoundRobinRunner node_runner(&node, kRunnerThreadNum);
    TestImpl(&node, &node_runner);
}

TEST_F(RRRunnerTest, MultiQueueMultiThread) {
    static constexpr auto  kRunnerThreadNum = 3;
    CRMultiQueueNode       node(kMessageNum);
    CRNodeRoundRobinRunner node_runner(&node, kRunnerThreadNum);
    TestImpl(&node, &node_runner);
}

void RRRunnerTest::TestImpl(CRNodeBase* node, CRNodeRunner* node_runner) {
    for (size_t i = 0; i < kMessageTypeNum; ++i) {
        for (size_t j = 0; j < kMessageNum; ++j) {
            EXPECT_EQ(mCount[i][j], 0);
        }
    }

    node->Subscribe<MessageForTest<0>>(&RRRunnerTest::MessageProcessorForTest<0>);
    node->Subscribe<MessageForTest<1>>(&RRRunnerTest::MessageProcessorForTest<1>);
    node->Subscribe<MessageForTest<2>>(&RRRunnerTest::MessageProcessorForTest<2>);
    node->Subscribe<MessageForTest<3>>(&RRRunnerTest::MessageProcessorForTest<3>);
    node->Subscribe<MessageForTest<4>>(&RRRunnerTest::MessageProcessorForTest<4>);
    node->Subscribe<MessageForTest<5>>(&RRRunnerTest::MessageProcessorForTest<5>);
    node->Subscribe<MessageForTest<6>>(&RRRunnerTest::MessageProcessorForTest<6>);
    node_runner->Run();

    for (size_t i = 0; i < kMessageNum; ++i) {
        auto message0 = std::make_shared<MessageForTest<0>>(i, this);
        auto message1 = std::make_shared<MessageForTest<1>>(i, this);
        auto message2 = std::make_shared<MessageForTest<2>>(i, this);
        auto message3 = std::make_shared<MessageForTest<3>>(i, this);
        auto message4 = std::make_shared<MessageForTest<4>>(i, this);
        auto message5 = std::make_shared<MessageForTest<5>>(i, this);
        auto message6 = std::make_shared<MessageForTest<6>>(i, this);

        CRMessageBase::Dispatch(message0);
        CRMessageBase::Dispatch(message1);
        CRMessageBase::Dispatch(message2);
        CRMessageBase::Dispatch(message3);
        CRMessageBase::Dispatch(message4);
        CRMessageBase::Dispatch(message5);
        CRMessageBase::Dispatch(message6);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (size_t i = 0; i < kMessageTypeNum; ++i) {
        for (size_t j = 0; j < kMessageNum; ++j) {
            EXPECT_EQ(mCount[i][j], 1);
        }
    }
}
