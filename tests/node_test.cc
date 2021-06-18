#include <thread>

#include "cris/core/node/base.h"
#include "cris/core/node/multi_queue_node.h"
#include "gtest/gtest.h"

namespace cris::core {

class TrivialNodeForTest : public CRNode {
    CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) override { return nullptr; }

   private:
    void SubscribeHandler(std::string &&                                  message_name,
                          std::function<void(const CRMessageBasePtr &)> &&callback) override {}

    std::vector<CRMessageQueue *> GetNodeQueues() override { return {}; }
};

TEST(NodeTest, WaitAndUnblock) {
    std::atomic<int>         unblocked{0};
    std::vector<std::thread> threads{};
    constexpr size_t         kThreadNum = 4;

    TrivialNodeForTest node;

    for (size_t i = 0; i < kThreadNum; ++i) {
        threads.emplace_back([&unblocked, &node]() {
            node.WaitForMessage(std::chrono::seconds(60));
            unblocked.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(unblocked.load(), 0);
    node.Kick();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(unblocked.load(), kThreadNum);
    for (auto &&thread : threads) {
        thread.join();
    }
}

TEST(NodeTest, WaitAndTimeout) {
    std::atomic<int>         unblocked{0};
    std::vector<std::thread> threads{};
    constexpr size_t         kThreadNum = 4;

    TrivialNodeForTest node;

    for (size_t i = 0; i < kThreadNum; ++i) {
        threads.emplace_back([&unblocked, &node]() {
            node.WaitForMessage(std::chrono::seconds(1));
            unblocked.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(unblocked.load(), 0);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(unblocked.load(), kThreadNum);
    for (auto &&thread : threads) {
        thread.join();
    }
}

template<int idx>
struct TestMessage : public CRMessage<TestMessage<idx>> {};

TEST(NodeTest, MultiQueueNode) {
    CRMultiQueueNode<>            node(1);
    constexpr size_t              kNumOfTopics = 4;
    std::array<int, kNumOfTopics> received{0};

    node.Subscribe<TestMessage<0>>(
        [&received](const std::shared_ptr<TestMessage<0>> &) { ++received[0]; });
    node.Subscribe<TestMessage<1>>(
        [&received](const std::shared_ptr<TestMessage<1>> &) { ++received[1]; });
    node.Subscribe<TestMessage<2>>(
        [&received](const std::shared_ptr<TestMessage<2>> &) { ++received[2]; });
    node.Subscribe<TestMessage<3>>(
        [&received](const std::shared_ptr<TestMessage<3>> &) { ++received[3]; });

    {
        constexpr int message_type_idx = 0;
        auto          message          = std::make_shared<TestMessage<message_type_idx>>();
        CRMessageBase::Dispatch(message);
        node.MessageQueueMapper(message)->PopAndProcess(false);

        EXPECT_EQ(received[message_type_idx], 1);
    }

    {
        constexpr int message_type_idx = 1;
        auto          message          = std::make_shared<TestMessage<message_type_idx>>();
        CRMessageBase::Dispatch(message);
        node.MessageQueueMapper(message)->PopAndProcess(false);

        EXPECT_EQ(received[message_type_idx], 1);
    }

    {
        constexpr int message_type_idx = 2;
        auto          message          = std::make_shared<TestMessage<message_type_idx>>();
        CRMessageBase::Dispatch(message);
        node.MessageQueueMapper(message)->PopAndProcess(false);

        EXPECT_EQ(received[message_type_idx], 1);
    }

    {
        constexpr int message_type_idx = 3;
        auto          message          = std::make_shared<TestMessage<message_type_idx>>();
        CRMessageBase::Dispatch(message);
        node.MessageQueueMapper(message)->PopAndProcess(false);

        EXPECT_EQ(received[message_type_idx], 1);
    }
}

}  // namespace cris::core
