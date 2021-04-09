#include "gtest/gtest.h"
#include "message/base.h"
#include "node/single_queue_node.h"

namespace cris::core {

struct TestMessageType1 : public CRMessage<TestMessageType1> {
    explicit TestMessageType1(int value) : mValue(value) {}

    int mValue;
};

struct SimpleTestNode : public CRSingleQueueNode {
    explicit SimpleTestNode(size_t queue_capacity)
        : CRSingleQueueNode(
              queue_capacity,
              std::bind(&SimpleTestNode::QueueCallback, this, std::placeholders::_1)) {}

    void Process() { mQueue.PopAndProcess(false); }

    void QueueCallback(const CRMessageBasePtr &message) {
        auto callback_search = mCallbacks.find(message->GetMessageTypeName());
        if (callback_search == mCallbacks.end()) {
            return;
        }
        return callback_search->second(message);
    }

   private:
    void SubscribeHandler(std::string &&                                  message_name,
                          std::function<void(const CRMessageBasePtr &)> &&callback) override {
        mCallbacks.emplace(message_name, std::move(callback));
    }

    std::map<std::string, std::function<void(const CRMessageBasePtr &)>> mCallbacks;
};

TEST(MessageTest, Basics) {
    // name
    CRMessageBasePtr message = std::make_shared<TestMessageType1>(1);
    EXPECT_EQ(CRMessageBase::GetMessageTypeName<TestMessageType1>(),
              GetTypeName<TestMessageType1>());
    EXPECT_EQ(message->GetMessageTypeName(), GetTypeName<TestMessageType1>());

    // empty dispatch
    CRMessageBase::Dispatch(message);
}

TEST(MessageTest, Subscribe) {
    CRMessageBasePtr message = std::make_shared<TestMessageType1>(1);

    // Dispatch message
    {
        int            count = 0;
        int            value = 0;
        SimpleTestNode node(1);

        node.Subscribe<TestMessageType1>([&](const CRMessageBasePtr &message) {
            ++count;
            value = static_pointer_cast<TestMessageType1>(message)->mValue;
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
        node1.Subscribe<TestMessageType1>([&](const CRMessageBasePtr &message) {
            ++count1;
            value1 = static_pointer_cast<TestMessageType1>(message)->mValue;
        });

        int            count2 = 0;
        int            value2 = 0;
        SimpleTestNode node2(1);
        node2.Subscribe<TestMessageType1>([&](const CRMessageBasePtr &message) {
            ++count2;
            value2 = static_pointer_cast<TestMessageType1>(message)->mValue;
        });

        int            count3 = 0;
        int            value3 = 0;
        SimpleTestNode node3(1);
        node3.Subscribe<TestMessageType1>([&](const CRMessageBasePtr &message) {
            ++count3;
            value3 = static_pointer_cast<TestMessageType1>(message)->mValue;
        });

        CRMessageBasePtr message1 = std::make_shared<TestMessageType1>(100);
        CRMessageBase::Dispatch(message1);

        node1.Process();
        EXPECT_EQ(count1, 1);
        EXPECT_EQ(value1, static_pointer_cast<TestMessageType1>(message1)->mValue);

        node2.Process();
        EXPECT_EQ(count2, 1);
        EXPECT_EQ(value2, static_pointer_cast<TestMessageType1>(message1)->mValue);

        node3.Process();
        EXPECT_EQ(count3, 1);
        EXPECT_EQ(value3, static_pointer_cast<TestMessageType1>(message1)->mValue);

        CRMessageBasePtr message2 = std::make_shared<TestMessageType1>(200);
        CRMessageBase::Dispatch(message2);

        node1.Process();
        EXPECT_EQ(count1, 2);
        EXPECT_EQ(value1, static_pointer_cast<TestMessageType1>(message2)->mValue);

        node2.Process();
        EXPECT_EQ(count2, 2);
        EXPECT_EQ(value2, static_pointer_cast<TestMessageType1>(message2)->mValue);

        node3.Process();
        EXPECT_EQ(count3, 2);
        EXPECT_EQ(value3, static_pointer_cast<TestMessageType1>(message2)->mValue);
    }

    // empty dispatch
    CRMessageBase::Dispatch(message);
}

}  // namespace cris::core
