#include "cris/core/message.h"

#include "cris/core/node.h"

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

TEST(MessageTest, Basics) {
    CRNode publisher;

    // name
    CRMessageBasePtr message = std::make_shared<TestMessage<1>>(1);
    EXPECT_EQ(message->GetMessageTypeName(), GetTypeName<TestMessage<1>>());
}

TEST(MessageTest, DeliveredTime) {
    CRNode publisher;

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

    CRNode node12;
    CRNode node23;
    CRNode node13;

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
        CRNode node;
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
