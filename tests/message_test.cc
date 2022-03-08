#include "cris/core/message/base.h"
#include "cris/core/node/single_queue_node.h"

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

struct SimpleTestNode : public CRSingleQueueNode<> {
    explicit SimpleTestNode(std::size_t queue_capacity)
        : CRSingleQueueNode<>(queue_capacity, std::bind(&SimpleTestNode::QueueCallback, this, std::placeholders::_1)) {}

    void Process() { queue_.PopAndProcess(false); }

    void QueueCallback(const CRMessageBasePtr& message) {
        auto callback_search = callbacks_.find(message->GetChannelId());
        if (callback_search == callbacks_.end()) {
            return;
        }
        return callback_search->second(message);
    }

   private:
    using callback_map_t =
        std::unordered_map<channel_id_t, std::function<void(const CRMessageBasePtr&)>, boost::hash<channel_id_t>>;

    void SubscribeHandler(const channel_id_t channel_id, std::function<void(const CRMessageBasePtr&)>&& callback)
        override {
        callbacks_.emplace(channel_id, std::move(callback));
    }

    callback_map_t callbacks_;
};

TEST(MessageTest, Basics) {
    // name
    CRMessageBasePtr message = std::make_shared<TestMessage<1>>(1);
    EXPECT_EQ(message->GetMessageTypeName(), GetTypeName<TestMessage<1>>());

    // empty dispatch
    CRMessageBase::Dispatch(message);
}

TEST(MessageTest, Subscribe) {
    channel_subid_t  channel_subid = 1;
    CRMessageBasePtr message       = std::make_shared<TestMessage<1>>(1);
    message->SetChannelSubId(channel_subid);

    // Dispatch message
    {
        int            count = 0;
        int            value = 0;
        SimpleTestNode node(1);

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

        CRMessageBase::Dispatch(message);
        node.Process();
        EXPECT_EQ(count, 1);
        EXPECT_EQ(value, 1);
    }

    // empty dispatch
    CRMessageBase::Dispatch(message);

    // multiple subscriber
    {
        int            count1 = 0;
        int            value1 = 0;
        SimpleTestNode node1(1);
        node1.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr& message) {
            ++count1;
            value1 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        int            count2 = 0;
        int            value2 = 0;
        SimpleTestNode node2(1);
        node2.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr& message) {
            ++count2;
            value2 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        int            count3 = 0;
        int            value3 = 0;
        SimpleTestNode node3(1);
        node3.Subscribe<TestMessage<1>>(channel_subid, [&](const CRMessageBasePtr& message) {
            ++count3;
            value3 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        CRMessageBasePtr message1 = std::make_shared<TestMessage<1>>(100);
        message1->SetChannelSubId(channel_subid);
        CRMessageBase::Dispatch(message1);

        node1.Process();
        EXPECT_EQ(count1, 1);
        EXPECT_EQ(value1, static_pointer_cast<TestMessage<1>>(message1)->value_);

        node2.Process();
        EXPECT_EQ(count2, 1);
        EXPECT_EQ(value2, static_pointer_cast<TestMessage<1>>(message1)->value_);

        node3.Process();
        EXPECT_EQ(count3, 1);
        EXPECT_EQ(value3, static_pointer_cast<TestMessage<1>>(message1)->value_);

        CRMessageBasePtr message2 = std::make_shared<TestMessage<1>>(200);
        message2->SetChannelSubId(channel_subid);
        CRMessageBase::Dispatch(message2);

        node1.Process();
        EXPECT_EQ(count1, 2);
        EXPECT_EQ(value1, static_pointer_cast<TestMessage<1>>(message2)->value_);

        node2.Process();
        EXPECT_EQ(count2, 2);
        EXPECT_EQ(value2, static_pointer_cast<TestMessage<1>>(message2)->value_);

        node3.Process();
        EXPECT_EQ(count3, 2);
        EXPECT_EQ(value3, static_pointer_cast<TestMessage<1>>(message2)->value_);
    }

    // empty dispatch
    CRMessageBase::Dispatch(message);
}

TEST(MessageTest, DeliveredTime) {
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

    SimpleTestNode node12(1);
    SimpleTestNode node23(1);
    SimpleTestNode node13(1);

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
            auto message    = std::make_shared<TestMessage<100>>(0);
            message->SetChannelSubId(channel_subid);
            CRMessageBase::Dispatch(message);
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), start_time);
        }

        {
            auto start_time = GetSystemTimestampNsec();
            auto message    = std::make_shared<TestMessage<200>>(0);
            message->SetChannelSubId(channel_subid);
            CRMessageBase::Dispatch(message);
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(channel_subid), start_time);
        }

        {
            auto start_time = GetSystemTimestampNsec();
            auto message    = std::make_shared<TestMessage<300>>(0);
            message->SetChannelSubId(channel_subid);
            CRMessageBase::Dispatch(message);
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
        SimpleTestNode node(1);
        node.Subscribe<TestMessage<100>>(channel_subid_2, [](auto&&) {});
        auto start_time = GetSystemTimestampNsec();
        auto message    = std::make_shared<TestMessage<100>>(0);
        message->SetChannelSubId(channel_subid_2);
        CRMessageBase::Dispatch(message);
        auto end_time = GetSystemTimestampNsec();

        // The second subchannel was updated while the first one remains unchanged
        EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid_2), start_time);
        EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid_2), end_time);
        EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(channel_subid), start_time);
    }
}

}  // namespace cris::core
