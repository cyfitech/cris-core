#include "cris/core/message/base.h"
#include "cris/core/node/multi_queue_node.h"

#include "gtest/gtest.h"

#include <boost/functional/hash.hpp>

#include <cstdlib>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>

namespace cris::core {

using channel_subid_t = CRMessageBase::channel_subid_t;

template<int idx>
struct TestMessage : public CRMessage<TestMessage<idx>> {
    explicit TestMessage(int value) : value_(value) {}

    int value_;
};

struct TestNode : public CRMultiQueueNode<> {
    using Base = CRMultiQueueNode<>;
    using Base::Base;

    void Process() {
        for (auto& queue : GetNodeQueues()) {
            while (!queue->IsEmpty()) {
                queue->PopAndProcess(false);
            }
        }
    }
};

TEST(MessageTest, Basics) {
    CRMultiQueueNode publisher(0);

    // name
    CRMessageBasePtr message = std::make_shared<TestMessage<1>>(1);
    EXPECT_EQ(message->GetMessageTypeName(), GetTypeName<TestMessage<1>>());

    // Publishing while nobody subscribing.
    publisher.Publish(CRMessageBase::kDefaultChannelSubID, std::move(message));
}

TEST(MessageTest, Subscribe) {
    CRMultiQueueNode publisher(0);

    channel_subid_t channel_subid = 1;

    // Publishing message
    {
        int      count = 0;
        int      value = 0;
        TestNode node(1);

        // Different subid, never called
        node.Subscribe<TestMessage<1>>(999, [&](const CRMessageBasePtr&) {
            // never enters
            std::abort();
        });

        node.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr& message) {
            ++count;
            value = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        // duplicate subscription, first one wins
        node.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr&) {
            // never enters
            std::abort();
        });

        publisher.Publish(channel_subid, std::make_shared<TestMessage<1>>(1));
        node.Process();
        EXPECT_EQ(count, 1);
        EXPECT_EQ(value, 1);
    }

    // Publishing while nobody subscribing.
    publisher.Publish(channel_subid, std::make_shared<TestMessage<1>>(1));

    // multiple subscriber
    {
        int      count1 = 0;
        int      value1 = 0;
        TestNode node1(1);
        node1.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr& message) {
            ++count1;
            value1 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        int      count2 = 0;
        int      value2 = 0;
        TestNode node2(1);
        node2.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr& message) {
            ++count2;
            value2 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        int      count3 = 0;
        int      value3 = 0;
        TestNode node3(1);
        node3.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr& message) {
            ++count3;
            value3 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        const int msg1_value = 100;
        publisher.Publish(channel_subid, std::make_shared<TestMessage<1>>(msg1_value));

        node1.Process();
        EXPECT_EQ(count1, 1);
        EXPECT_EQ(value1, msg1_value);

        node2.Process();
        EXPECT_EQ(count2, 1);
        EXPECT_EQ(value2, msg1_value);

        node3.Process();
        EXPECT_EQ(count3, 1);
        EXPECT_EQ(value3, msg1_value);

        const int msg2_value = 200;
        publisher.Publish(channel_subid, std::make_shared<TestMessage<1>>(msg2_value));

        node1.Process();
        EXPECT_EQ(count1, 2);
        EXPECT_EQ(value1, msg2_value);

        node2.Process();
        EXPECT_EQ(count2, 2);
        EXPECT_EQ(value2, msg2_value);

        node3.Process();
        EXPECT_EQ(count3, 2);
        EXPECT_EQ(value3, msg2_value);
    }

    // Publishing while nobody subscribing.
    publisher.Publish(channel_subid, std::make_shared<TestMessage<1>>(1));
}

TEST(MessageTest, DeliveredTime) {
    CRMultiQueueNode publisher(0);

    channel_subid_t               channel_subid     = 1;
    channel_subid_t               channel_subid_2   = 2;
    constexpr cr_timestamp_nsec_t kDefaultTimestamp = 0;

    // Unknown/Unsent Message Type
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid_2), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid_2), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid_2), kDefaultTimestamp);

    TestNode node12(1);
    TestNode node23(1);
    TestNode node13(1);

    node12.Subscribe<TestMessage<100>>(channel_subid, [](auto&&) {});
    node12.Subscribe<TestMessage<200>>(channel_subid, [](auto&&) {});

    node23.Subscribe<TestMessage<200>>(channel_subid, [](auto&&) {});
    node23.Subscribe<TestMessage<300>>(channel_subid, [](auto&&) {});

    node13.Subscribe<TestMessage<100>>(channel_subid, [](auto&&) {});
    node13.Subscribe<TestMessage<300>>(channel_subid, [](auto&&) {});

    constexpr auto kRepeatTime = 5;

    for (int i = 0; i < kRepeatTime; ++i) {
        {
            auto start_time = GetSystemTimestampNsec();
            publisher.Publish(channel_subid, std::make_shared<TestMessage<100>>(0));
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), start_time);
        }

        {
            auto start_time = GetSystemTimestampNsec();
            publisher.Publish(channel_subid, std::make_shared<TestMessage<200>>(0));
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), start_time);
        }

        {
            auto start_time = GetSystemTimestampNsec();
            publisher.Publish(channel_subid, std::make_shared<TestMessage<300>>(0));
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid), start_time);
        }
    }

    // Other subchannels remain unchanged
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid_2), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid_2), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid_2), kDefaultTimestamp);

    {
        TestNode node(1);
        node.Subscribe<TestMessage<100>>(channel_subid_2, [](auto&&) {});
        auto start_time = GetSystemTimestampNsec();
        publisher.Publish(channel_subid_2, std::make_shared<TestMessage<100>>(0));
        auto end_time = GetSystemTimestampNsec();

        // The second subchannel was updated while the first one remains unchanged
        EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid_2), start_time);
        EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid_2), end_time);
        EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), start_time);
    }
}

}  // namespace cris::core
