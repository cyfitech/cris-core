#include "cris/core/message/base.h"
#include "cris/core/node/single_queue_node.h"

#include "gtest/gtest.h"

#include <cstdlib>
#include <memory>
#include <unordered_map>

namespace cris::core {

template<int idx>
struct TestMessage : public CRMessage<TestMessage<idx>> {
    explicit TestMessage(int value) : value_(value) {}

    int value_;
};

struct SimpleTestNode : public CRSingleQueueNode<> {
    explicit SimpleTestNode(size_t queue_capacity)
        : CRSingleQueueNode<>(queue_capacity, std::bind(&SimpleTestNode::QueueCallback, this, std::placeholders::_1)) {}

    void Process() { queue_.PopAndProcess(false); }

    void QueueCallback(const CRMessageBasePtr& message) {
        auto callback_search = callbacks_.find(message->GetMessageTypeIndex());
        if (callback_search == callbacks_.end()) {
            return;
        }
        return callback_search->second(message);
    }

   private:
    void SubscribeHandler(const std::type_index message_type, std::function<void(const CRMessageBasePtr&)>&& callback)
        override {
        callbacks_.emplace(message_type, std::move(callback));
    }

    std::unordered_map<std::type_index, std::function<void(const CRMessageBasePtr&)>> callbacks_;
};

TEST(MessageTest, Basics) {
    // name
    CRMessageBasePtr message = std::make_shared<TestMessage<1>>(1);
    EXPECT_EQ(message->GetMessageTypeName(), GetTypeName<TestMessage<1>>());

    // empty dispatch
    CRMessageBase::Dispatch(message);
}

TEST(MessageTest, Subscribe) {
    CRMessageBasePtr message = std::make_shared<TestMessage<1>>(1);

    // Dispatch message
    {
        int            count = 0;
        int            value = 0;
        SimpleTestNode node(1);

        node.Subscribe<TestMessage<1>>([&](const CRMessageBasePtr& message) {
            ++count;
            value = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        // duplicate subscription, first one wins
        node.Subscribe<TestMessage<1>>([&](const CRMessageBasePtr&) {
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
        node1.Subscribe<TestMessage<1>>([&](const CRMessageBasePtr& message) {
            ++count1;
            value1 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        int            count2 = 0;
        int            value2 = 0;
        SimpleTestNode node2(1);
        node2.Subscribe<TestMessage<1>>([&](const CRMessageBasePtr& message) {
            ++count2;
            value2 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        int            count3 = 0;
        int            value3 = 0;
        SimpleTestNode node3(1);
        node3.Subscribe<TestMessage<1>>([&](const CRMessageBasePtr& message) {
            ++count3;
            value3 = static_pointer_cast<TestMessage<1>>(message)->value_;
        });

        CRMessageBasePtr message1 = std::make_shared<TestMessage<1>>(100);
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
    constexpr cr_timestamp_nsec_t kDefaultTimestamp = 0;

    // Unknown/Unsent Message Type
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(), kDefaultTimestamp);
    EXPECT_EQ(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(), kDefaultTimestamp);

    SimpleTestNode node12(1);
    SimpleTestNode node23(1);
    SimpleTestNode node13(1);

    node12.Subscribe<TestMessage<100>>([](auto&&) {});
    node12.Subscribe<TestMessage<200>>([](auto&&) {});

    node23.Subscribe<TestMessage<200>>([](auto&&) {});
    node23.Subscribe<TestMessage<300>>([](auto&&) {});

    node13.Subscribe<TestMessage<100>>([](auto&&) {});
    node13.Subscribe<TestMessage<300>>([](auto&&) {});

    constexpr auto kRepeatTime = 5;

    for (int i = 0; i < kRepeatTime; ++i) {
        {
            auto start_time = GetSystemTimestampNsec();
            CRMessageBase::Dispatch(std::make_shared<TestMessage<100>>(0));
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(), start_time);
        }

        {
            auto start_time = GetSystemTimestampNsec();
            CRMessageBase::Dispatch(std::make_shared<TestMessage<200>>(0));
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(), start_time);
        }

        {
            auto start_time = GetSystemTimestampNsec();
            CRMessageBase::Dispatch(std::make_shared<TestMessage<300>>(0));
            auto end_time = GetSystemTimestampNsec();

            EXPECT_GE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<300>>(), end_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<100>>(), start_time);
            EXPECT_LE(CRMessageBase::GetLatestDeliveredTime<TestMessage<200>>(), start_time);
        }
    }
}

}  // namespace cris::core
