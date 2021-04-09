#include "cris/core/message/lock_queue.h"

#include "gtest/gtest.h"

namespace cris::core {

struct TestMessageType1 : public CRMessage<TestMessageType1> {
    explicit TestMessageType1(int value) : mValue(value) {}

    int mValue;
};

struct TestQueue {
    void Test();

    virtual CRMessageQueue *GetQueue() = 0;

    virtual ~TestQueue() = default;

    void Processor(const CRMessageBasePtr &message) {
        mLastValue = static_pointer_cast<TestMessageType1>(message)->mValue;
    };

    int mLastValue{kNoLastValue};

    static constexpr size_t kQueueSize   = 10;
    static constexpr int    kNoLastValue = -1;
    static constexpr size_t kTestBatch   = 37;  // some random number
};

struct TestLockQueue : public TestQueue {
    TestLockQueue()
        : mQueue(kQueueSize,
                 nullptr,
                 std::bind(&TestQueue::Processor, this, std::placeholders::_1)) {}

    CRMessageQueue *GetQueue() override { return &mQueue; }

    CRMessageLockQueue mQueue;
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
    EXPECT_EQ(mLastValue, kNoLastValue);
    GetQueue()->PopAndProcess(true);
    EXPECT_EQ(mLastValue, kNoLastValue);
    EXPECT_TRUE(GetQueue()->IsEmpty());
    EXPECT_FALSE(GetQueue()->IsFull());

    // Add and then pop for each value
    for (size_t i = 0; i < kTestBatch; ++i) {
        EXPECT_TRUE(GetQueue()->IsEmpty());
        GetQueue()->AddMessage(std::make_shared<TestMessageType1>(i));
        EXPECT_FALSE(GetQueue()->IsEmpty());
        GetQueue()->PopAndProcess(false);
        EXPECT_TRUE(GetQueue()->IsEmpty());
        EXPECT_EQ(mLastValue, i);
        mLastValue = kNoLastValue;
    }
    EXPECT_TRUE(GetQueue()->IsEmpty());
    GetQueue()->PopAndProcess(false);
    EXPECT_EQ(mLastValue, kNoLastValue);

    // Add and then pop latest for each value
    for (size_t i = 0; i < kTestBatch; ++i) {
        GetQueue()->AddMessage(std::make_shared<TestMessageType1>(i));
        GetQueue()->PopAndProcess(true);
        EXPECT_EQ(mLastValue, i);
        mLastValue = kNoLastValue;
    }
    EXPECT_TRUE(GetQueue()->IsEmpty());
    GetQueue()->PopAndProcess(true);
    EXPECT_EQ(mLastValue, kNoLastValue);

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
        EXPECT_EQ(mLastValue, kTestBatch - kQueueSize + i);
        mLastValue = kNoLastValue;
    }
    EXPECT_TRUE(GetQueue()->IsEmpty());
    GetQueue()->PopAndProcess(false);
    EXPECT_EQ(mLastValue, kNoLastValue);

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
    EXPECT_EQ(mLastValue, kTestBatch - 1);
    mLastValue = kNoLastValue;
    GetQueue()->PopAndProcess(true);
    EXPECT_EQ(mLastValue, kNoLastValue);
}

}  // namespace cris::core
