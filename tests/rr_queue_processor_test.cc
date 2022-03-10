#include "cris/core/message/base.h"
#include "cris/core/node/multi_queue_node.h"
#include "cris/core/node/single_queue_node.h"
#include "cris/core/node_runner/runner.h"

#include "gtest/gtest.h"

#include <boost/functional/hash.hpp>

#include <cstddef>
#include <cstring>
#include <unordered_map>

using namespace cris::core;

static constexpr std::size_t kMessageTypeNum = 7;
static constexpr std::size_t kMessageNum     = 100;

class CRSingleQueueNodeForTest : public CRSingleQueueNode<> {
   public:
    CRSingleQueueNodeForTest()
        : CRSingleQueueNode<>(
              kMessageTypeNum * kMessageNum,
              std::bind(&CRSingleQueueNodeForTest::QueueProcessor, this, std::placeholders::_1)) {}

   private:
    void SubscribeHandler(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback)
        override {
        subscriptions_.emplace(channel, std::move(callback));
    }

    void QueueProcessor(const CRMessageBasePtr& message) { return subscriptions_[message->GetChannelId()](message); }

    using callback_map_t =
        std::unordered_map<channel_id_t, std::function<void(const CRMessageBasePtr&)>, boost::hash<channel_id_t>>;

    callback_map_t subscriptions_;
};

class CRMultiQueueNodeForTest : public CRMultiQueueNode<> {
   public:
    CRMultiQueueNodeForTest() : CRMultiQueueNode<>(kMessageNum) {}
};

class RRQueueProcessorTest : public testing::Test {
   public:
    void SetUp() override { std::memset(count_, 0, sizeof(count_)); }

    template<int idx>
    struct MessageForTest : public CRMessage<MessageForTest<idx>> {
        MessageForTest(int value, RRQueueProcessorTest* test) : value_(value), test_(test) {}

        void Process() { ++test_->count_[idx][value_]; }

        int                   value_;
        RRQueueProcessorTest* test_;
    };

    template<int idx>
    static void MessageProcessorForTest(const std::shared_ptr<MessageForTest<idx>>& message) {
        message->Process();
    }

    void QueueProcessorTestImpl(const std::vector<CRNodeBase*>& nodes, CRNodeQueueProcessor* queue_processor);

    unsigned count_[kMessageTypeNum][kMessageNum];
};

template<class node_t>
static std::vector<CRNodeBase*> GetNodePtrs(std::vector<node_t>& nodes) {
    std::vector<CRNodeBase*> node_ptrs;
    for (auto&& node : nodes) {
        node_ptrs.push_back(static_cast<CRNodeBase*>(&node));
    }
    return node_ptrs;
}

TEST_F(RRQueueProcessorTest, SingleQueueSingleThread) {
    static constexpr std::size_t kNodeNum                 = 1;
    static constexpr std::size_t kQueueProcessorThreadNum = 1;

    std::vector<CRSingleQueueNodeForTest> nodes(kNodeNum);

    auto node_ptrs = GetNodePtrs(nodes);

    CRNodeRoundRobinQueueProcessor node_runner(node_ptrs, kQueueProcessorThreadNum);
    QueueProcessorTestImpl(node_ptrs, &node_runner);
}

TEST_F(RRQueueProcessorTest, SingleQueueMultiThread) {
    static constexpr std::size_t kNodeNum                 = 1;
    static constexpr std::size_t kQueueProcessorThreadNum = 3;

    std::vector<CRSingleQueueNodeForTest> nodes(kNodeNum);

    auto node_ptrs = GetNodePtrs(nodes);

    CRNodeRoundRobinQueueProcessor node_runner(node_ptrs, kQueueProcessorThreadNum);
    QueueProcessorTestImpl(node_ptrs, &node_runner);
}

TEST_F(RRQueueProcessorTest, MultiQueueSingleThread) {
    static constexpr std::size_t kNodeNum                 = 3;
    static constexpr std::size_t kQueueProcessorThreadNum = 1;

    std::vector<CRMultiQueueNodeForTest> nodes(kNodeNum);

    auto node_ptrs = GetNodePtrs(nodes);

    CRNodeRoundRobinQueueProcessor node_runner(node_ptrs, kQueueProcessorThreadNum);
    QueueProcessorTestImpl(node_ptrs, &node_runner);
}

TEST_F(RRQueueProcessorTest, MultiQueueMultiThread) {
    static constexpr std::size_t kNodeNum                 = 3;
    static constexpr std::size_t kQueueProcessorThreadNum = 3;

    std::vector<CRMultiQueueNodeForTest> nodes(kNodeNum);

    auto node_ptrs = GetNodePtrs(nodes);

    CRNodeRoundRobinQueueProcessor node_runner(node_ptrs, kQueueProcessorThreadNum);
    QueueProcessorTestImpl(node_ptrs, &node_runner);
}

void RRQueueProcessorTest::QueueProcessorTestImpl(
    const std::vector<CRNodeBase*>& nodes,
    CRNodeQueueProcessor*           queue_processor) {
    constexpr CRMessageBase::channel_subid_t kChannelSubIDForTest = 1;

    for (std::size_t i = 0; i < kMessageTypeNum; ++i) {
        for (std::size_t j = 0; j < kMessageNum; ++j) {
            EXPECT_EQ(count_[i][j], 0);
        }
    }

    for (auto* node : nodes) {
        node->Subscribe<MessageForTest<0>>(kChannelSubIDForTest, &RRQueueProcessorTest::MessageProcessorForTest<0>);
        node->Subscribe<MessageForTest<1>>(kChannelSubIDForTest, &RRQueueProcessorTest::MessageProcessorForTest<1>);
        node->Subscribe<MessageForTest<2>>(kChannelSubIDForTest, &RRQueueProcessorTest::MessageProcessorForTest<2>);
        node->Subscribe<MessageForTest<3>>(kChannelSubIDForTest, &RRQueueProcessorTest::MessageProcessorForTest<3>);
        node->Subscribe<MessageForTest<4>>(kChannelSubIDForTest, &RRQueueProcessorTest::MessageProcessorForTest<4>);
        node->Subscribe<MessageForTest<5>>(kChannelSubIDForTest, &RRQueueProcessorTest::MessageProcessorForTest<5>);
        node->Subscribe<MessageForTest<6>>(kChannelSubIDForTest, &RRQueueProcessorTest::MessageProcessorForTest<6>);
    }
    queue_processor->Run();

    for (std::size_t i = 0; i < kMessageNum; ++i) {
        auto message0 = std::make_shared<MessageForTest<0>>(i, this);
        auto message1 = std::make_shared<MessageForTest<1>>(i, this);
        auto message2 = std::make_shared<MessageForTest<2>>(i, this);
        auto message3 = std::make_shared<MessageForTest<3>>(i, this);
        auto message4 = std::make_shared<MessageForTest<4>>(i, this);
        auto message5 = std::make_shared<MessageForTest<5>>(i, this);
        auto message6 = std::make_shared<MessageForTest<6>>(i, this);

        message0->SetChannelSubId(kChannelSubIDForTest);
        CRMessageBase::Dispatch(message0);
        message1->SetChannelSubId(kChannelSubIDForTest);
        CRMessageBase::Dispatch(message1);
        message2->SetChannelSubId(kChannelSubIDForTest);
        CRMessageBase::Dispatch(message2);
        message3->SetChannelSubId(kChannelSubIDForTest);
        CRMessageBase::Dispatch(message3);
        message4->SetChannelSubId(kChannelSubIDForTest);
        CRMessageBase::Dispatch(message4);
        message5->SetChannelSubId(kChannelSubIDForTest);
        CRMessageBase::Dispatch(message5);
        message6->SetChannelSubId(kChannelSubIDForTest);
        CRMessageBase::Dispatch(message6);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    queue_processor->Stop();
    queue_processor->Join();

    for (std::size_t i = 0; i < kMessageTypeNum; ++i) {
        for (std::size_t j = 0; j < kMessageNum; ++j) {
            EXPECT_EQ(count_[i][j], nodes.size());
        }
    }
}
