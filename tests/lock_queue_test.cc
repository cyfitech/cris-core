#include "cris/core/message/lock_queue.h"

#include "gtest/gtest.h"

namespace cris::core {

struct TestMessageType1 : public CRMessage<TestMessageType1> {
    explicit TestMessageType1(int value) : value_(value) {}

    int value_;
};

struct TestQueue {
    void Test();

    virtual CRMessageQueue *GetQueue() = 0;

    virtual ~TestQueue() = default;

    void Processor(const CRMessageBasePtr &message) {
        last_value_ = static_pointer_cast<TestMessageType1>(message)->value_;
    };

    int last_value_{kNoLastValue};

    static constexpr size_t kQueueSize   = 10;
    static constexpr int    kNoLastValue = -1;
    static constexpr size_t kTestBatch   = 37;  // some random number
};

struct TestLockQueue : public TestQueue {
    TestLockQueue() : queue_(kQueueSize, nullptr, std::bind(&TestQueue::Processor, this, std::placeholders::_1)) {}

    CRMessageQueue *GetQueue() override { return &queue_; }

    CRMessageLockQueue queue_;
};

TEST(LockQueueTest, Basics) {
    TestLockQueue test_queue;
    test_queue.Test();
}

void TestQueue::Test() {
    EXPECT_TRUE(GetQueue()->IsEmpty());
    EXPECT_FALSE(GetQueue()->IsFull());

    // Empty pop and process
    GetQueue()->PopAndProcess(false);
    EXPECT_EQ(last_value_, kNoLastValue);
    GetQueue()->PopAndProcess(true);
    EXPECT_EQ(last_value_, kNoLastValue);
    EXPECT_TRUE(GetQueue()->IsEmpty());
    EXPECT_FALSE(GetQueue()->IsFull());

    // Add and then pop for each value
    for (size_t i = 0; i < kTestBatch; ++i) {
        EXPECT_TRUE(GetQueue()->IsEmpty());
        GetQueue()->AddMessage(std::make_shared<TestMessageType1>(i));
        EXPECT_FALSE(GetQueue()->IsEmpty());
        GetQueue()->PopAndProcess(false);
        EXPECT_TRUE(GetQueue()->IsEmpty());
        EXPECT_EQ(last_value_, i);
        last_value_ = kNoLastValue;
    }
    EXPECT_TRUE(GetQueue()->IsEmpty());
    GetQueue()->PopAndProcess(false);
    EXPECT_EQ(last_value_, kNoLastValue);

    // Add and then pop latest for each value
    for (size_t i = 0; i < kTestBatch; ++i) {
        GetQueue()->AddMessage(std::make_shared<TestMessageType1>(i));
        GetQueue()->PopAndProcess(true);
        EXPECT_EQ(last_value_, i);
        last_value_ = kNoLastValue;
    }
    EXPECT_TRUE(GetQueue()->IsEmpty());
    GetQueue()->PopAndProcess(true);
    EXPECT_EQ(last_value_, kNoLastValue);

    // Add all and then pop for each value
    for (size_t i = 0; i < kTestBatch; ++i) {
        GetQueue()->AddMessage(std::make_shared<TestMessageType1>(i));
        EXPECT_EQ(GetQueue()->IsFull(), i + 1 >= kQueueSize);
        EXPECT_EQ(GetQueue()->Size(), std::min(i + 1, kQueueSize));
    }
    EXPECT_TRUE(GetQueue()->IsFull());
    // only last kQueueSize values remains
    for (size_t i = 0; i < kQueueSize; ++i) {
        EXPECT_FALSE(GetQueue()->IsEmpty());
        GetQueue()->PopAndProcess(false);
        EXPECT_EQ(GetQueue()->Size(), kQueueSize - i - 1);
        EXPECT_FALSE(GetQueue()->IsFull());
        EXPECT_EQ(last_value_, kTestBatch - kQueueSize + i);
        last_value_ = kNoLastValue;
    }
    EXPECT_TRUE(GetQueue()->IsEmpty());
    GetQueue()->PopAndProcess(false);
    EXPECT_EQ(last_value_, kNoLastValue);

    // Add all and then pop the latest
    for (size_t i = 0; i < kTestBatch; ++i) {
        GetQueue()->AddMessage(std::make_shared<TestMessageType1>(i));
    }
    EXPECT_TRUE(GetQueue()->IsFull());
    EXPECT_FALSE(GetQueue()->IsEmpty());
    GetQueue()->PopAndProcess(true);
    EXPECT_FALSE(GetQueue()->IsFull());
    EXPECT_TRUE(GetQueue()->IsEmpty());
    EXPECT_EQ(GetQueue()->Size(), 0);
    EXPECT_EQ(last_value_, kTestBatch - 1);
    last_value_ = kNoLastValue;
    GetQueue()->PopAndProcess(true);
    EXPECT_EQ(last_value_, kNoLastValue);
}

}  // namespace cris::core
