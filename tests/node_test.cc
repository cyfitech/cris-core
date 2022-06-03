#include "cris/core/node.h"

#include "gtest/gtest.h"

#include <thread>

namespace cris::core {

TEST(NodeTest, WaitAndUnblock) {
    std::atomic<int>         unblocked{0};
    std::vector<std::thread> threads{};
    constexpr std::size_t    kThreadNum = 4;

    CRNode node(1);

    for (std::size_t i = 0; i < kThreadNum; ++i) {
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
    for (auto&& thread : threads) {
        thread.join();
    }
}

TEST(NodeTest, WaitAndTimeout) {
    std::atomic<int>         unblocked{0};
    std::vector<std::thread> threads{};
    constexpr std::size_t    kThreadNum = 4;

    CRNode node(1);

    for (std::size_t i = 0; i < kThreadNum; ++i) {
        threads.emplace_back([&unblocked, &node]() {
            node.WaitForMessage(std::chrono::seconds(1));
            unblocked.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(unblocked.load(), 0);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(unblocked.load(), kThreadNum);
    for (auto&& thread : threads) {
        thread.join();
    }
}

template<int idx>
struct TestMessage : public CRMessage<TestMessage<idx>> {};

TEST(NodeTest, Basic) {
    CRNode                node(1);
    constexpr std::size_t kNumOfTopics     = 4;
    constexpr std::size_t kNumOfSubChannel = 4;

    std::array<std::array<int, kNumOfSubChannel>, kNumOfTopics> received{};

    for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
        node.Subscribe<TestMessage<0>>(
            channel_subid,
            [&received, channel_subid](const std::shared_ptr<TestMessage<0>>&) { ++received[0][channel_subid]; });
        node.Subscribe<TestMessage<1>>(
            channel_subid,
            [&received, channel_subid](const std::shared_ptr<TestMessage<1>>&) { ++received[1][channel_subid]; });
        node.Subscribe<TestMessage<2>>(
            channel_subid,
            [&received, channel_subid](const std::shared_ptr<TestMessage<2>>&) { ++received[2][channel_subid]; });
        node.Subscribe<TestMessage<3>>(
            channel_subid,
            [&received, channel_subid](const std::shared_ptr<TestMessage<3>>&) { ++received[3][channel_subid]; });
    }

    for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
        {
            constexpr int message_type_idx = 0;
            auto          message          = std::make_shared<TestMessage<message_type_idx>>();
            message->SetChannelSubId(channel_subid);

            EXPECT_EQ(received[message_type_idx][channel_subid], 0);

            CRMessageBase::Dispatch(message);
            node.MessageQueueMapper(message->GetChannelId())->PopAndProcess(false);

            EXPECT_EQ(received[message_type_idx][channel_subid], 1);
        }

        {
            constexpr int message_type_idx = 1;
            auto          message          = std::make_shared<TestMessage<message_type_idx>>();
            message->SetChannelSubId(channel_subid);

            EXPECT_EQ(received[message_type_idx][channel_subid], 0);

            CRMessageBase::Dispatch(message);
            node.MessageQueueMapper(message->GetChannelId())->PopAndProcess(false);

            EXPECT_EQ(received[message_type_idx][channel_subid], 1);
        }

        {
            constexpr int message_type_idx = 2;
            auto          message          = std::make_shared<TestMessage<message_type_idx>>();
            message->SetChannelSubId(channel_subid);

            EXPECT_EQ(received[message_type_idx][channel_subid], 0);

            CRMessageBase::Dispatch(message);
            node.MessageQueueMapper(message->GetChannelId())->PopAndProcess(false);

            EXPECT_EQ(received[message_type_idx][channel_subid], 1);
        }

        {
            constexpr int message_type_idx = 3;
            auto          message          = std::make_shared<TestMessage<message_type_idx>>();
            message->SetChannelSubId(channel_subid);

            EXPECT_EQ(received[message_type_idx][channel_subid], 0);

            CRMessageBase::Dispatch(message);
            node.MessageQueueMapper(message->GetChannelId())->PopAndProcess(false);

            EXPECT_EQ(received[message_type_idx][channel_subid], 1);
        }
    }
}

}  // namespace cris::core
