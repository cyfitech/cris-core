#include "cris/core/msg/node.h"

#include "gtest/gtest.h"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace cris::core {

using channel_subid_t = CRNode::channel_subid_t;

template<int idx>
struct TestMessage : public CRMessage<TestMessage<idx>> {
    explicit TestMessage(int value) : value_(value) {}

    int value_;
};

TEST(NodeTest, Basic) {
    using TestMessageType = TestMessage<1>;

    CRNode                publisher;
    const channel_subid_t channel_subid = 2;

    // Publishing while nobody subscribing.
    publisher.Publish(channel_subid, std::make_shared<TestMessageType>(1));

    {
        CRNode subscriber;
        auto   runner = std::make_unique<JobRunner>(JobRunner::Config{});
        subscriber.SetRunner(runner.get());

        auto cv_mutex = std::make_shared<std::mutex>();
        auto cv       = std::make_shared<std::condition_variable>();
        auto received = std::make_shared<bool>(false);

        const int message_value = 0;

        // Different type, never called.
        subscriber.Subscribe<TestMessage<10086>>(channel_subid, [](const auto&) { FAIL(); });

        // Different subid, never called.
        subscriber.Subscribe<TestMessageType>(10010, [](const auto&) { FAIL(); });

        subscriber.Subscribe<TestMessageType>(
            channel_subid,
            [message_value, cv_mutex, cv, received](const std::shared_ptr<TestMessageType>& msg) {
                std::unique_lock cv_lock(*cv_mutex);
                EXPECT_EQ(msg->value_, message_value);
                *received = true;
                cv->notify_all();
            });

        // Duplicate subscription, first one wins.
        subscriber.Subscribe<TestMessageType>(channel_subid, [&](const auto&) { FAIL(); });

        publisher.Publish(channel_subid, std::make_shared<TestMessageType>(message_value));

        std::unique_lock cv_lock(*cv_mutex);
        cv->wait(cv_lock, [received]() { return *received; });
    }

    // Publishing while nobody subscribing.
    publisher.Publish(channel_subid, std::make_shared<TestMessageType>(1));
}

TEST(NodeTest, MultipleChannels) {
    constexpr std::size_t kNumOfMsgTypes   = 4;
    constexpr std::size_t kNumOfSubChannel = 4;

    CRNode publisher;
    CRNode subscriber;
    auto   runner = std::make_unique<JobRunner>(JobRunner::Config{});
    subscriber.SetRunner(runner.get());

    struct Received {
        std::array<std::array<int, kNumOfSubChannel>, kNumOfMsgTypes> data_{};
    };

    auto cv_mutex = std::make_shared<std::mutex>();
    auto cv       = std::make_shared<std::condition_variable>();

    auto received = std::make_shared<Received>();

    for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
        subscriber.Subscribe<TestMessage<0>>(
            channel_subid,
            [received, channel_subid, cv_mutex, cv](const std::shared_ptr<TestMessage<0>>& msg) {
                std::unique_lock cv_lock(*cv_mutex);
                received->data_[0][channel_subid] = msg->value_;
                cv->notify_all();
            });
        subscriber.Subscribe<TestMessage<1>>(
            channel_subid,
            [received, channel_subid, cv_mutex, cv](const std::shared_ptr<TestMessage<1>>& msg) {
                std::unique_lock cv_lock(*cv_mutex);
                received->data_[1][channel_subid] = msg->value_;
                cv->notify_all();
            });
        subscriber.Subscribe<TestMessage<2>>(
            channel_subid,
            [received, channel_subid, cv_mutex, cv](const std::shared_ptr<TestMessage<2>>& msg) {
                std::unique_lock cv_lock(*cv_mutex);
                received->data_[2][channel_subid] = msg->value_;
                cv->notify_all();
            });
        subscriber.Subscribe<TestMessage<3>>(
            channel_subid,
            [received, channel_subid, cv_mutex, cv](const std::shared_ptr<TestMessage<3>>& msg) {
                std::unique_lock cv_lock(*cv_mutex);
                received->data_[3][channel_subid] = msg->value_;
                cv->notify_all();
            });
    }

    for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
        {
            constexpr int message_type_idx = 0;
            const int     message_value    = static_cast<int>(channel_subid + 1) * 100 + message_type_idx;

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], 0);

            publisher.Publish(channel_subid, std::make_shared<TestMessage<message_type_idx>>(message_value));

            std::unique_lock cv_lock(*cv_mutex);
            cv->wait(cv_lock, [received, channel_subid]() {
                return received->data_[message_type_idx][channel_subid] != 0;
            });

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], message_value);
        }

        {
            constexpr int message_type_idx = 1;
            const int     message_value    = static_cast<int>(channel_subid + 1) * 100 + message_type_idx;

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], 0);

            publisher.Publish(channel_subid, std::make_shared<TestMessage<message_type_idx>>(message_value));

            std::unique_lock cv_lock(*cv_mutex);
            cv->wait(cv_lock, [received, channel_subid]() {
                return received->data_[message_type_idx][channel_subid] != 0;
            });

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], message_value);
        }

        {
            constexpr int message_type_idx = 2;
            const int     message_value    = static_cast<int>(channel_subid + 1) * 100 + message_type_idx;

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], 0);

            publisher.Publish(channel_subid, std::make_shared<TestMessage<message_type_idx>>(message_value));

            std::unique_lock cv_lock(*cv_mutex);
            cv->wait(cv_lock, [received, channel_subid]() {
                return received->data_[message_type_idx][channel_subid] != 0;
            });

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], message_value);
        }

        {
            constexpr int message_type_idx = 3;
            const int     message_value    = static_cast<int>(channel_subid + 1) * 100 + message_type_idx;

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], 0);

            publisher.Publish(channel_subid, std::make_shared<TestMessage<message_type_idx>>(message_value));

            std::unique_lock cv_lock(*cv_mutex);
            cv->wait(cv_lock, [received, channel_subid]() {
                return received->data_[message_type_idx][channel_subid] != 0;
            });

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], message_value);
        }
    }
}

TEST(NodeTest, MultipleSubscriber) {
    constexpr std::size_t kNumOfSubscribers = 4;

    using TestMessageType = TestMessage<11>;

    CRNode                publisher;
    const channel_subid_t channel_subid = 12;

    struct Subscriber : public CRNode {
        using CRNode::CRNode;

        int received_{0};
    };

    std::vector<std::unique_ptr<Subscriber>> subscribers;

    auto runner = std::make_unique<JobRunner>(JobRunner::Config{});

    auto cv_mutex = std::make_shared<std::mutex>();
    auto cv       = std::make_shared<std::condition_variable>();

    for (std::size_t i = 0; i < kNumOfSubscribers; ++i) {
        auto subscriber = std::make_unique<Subscriber>();
        subscriber->SetRunner(runner.get());
        subscriber->Subscribe<TestMessageType>(
            channel_subid,
            [subscriber = subscriber.get(), cv_mutex, cv](const std::shared_ptr<TestMessageType>& message) {
                std::unique_lock cv_lock(*cv_mutex);
                subscriber->received_ = message->value_;
                cv->notify_all();
            });
        subscribers.push_back(std::move(subscriber));
    }

    {
        const int message_value = 100;

        for (auto& subscriber : subscribers) {
            subscriber->received_ = 0;
        }
        publisher.Publish(channel_subid, std::make_shared<TestMessageType>(message_value));
        std::unique_lock lock(*cv_mutex);

        for (auto& subscriber : subscribers) {
            cv->wait(lock, [subscriber = subscriber.get()]() { return subscriber->received_ != 0; });
            EXPECT_EQ(subscriber->received_, message_value);
        }
    }

    {
        const int message_value = 200;

        for (auto& subscriber : subscribers) {
            subscriber->received_ = 0;
        }
        publisher.Publish(channel_subid, std::make_shared<TestMessageType>(message_value));
        std::unique_lock lock(*cv_mutex);

        for (auto& subscriber : subscribers) {
            cv->wait(lock, [subscriber = subscriber.get()]() { return subscriber->received_ != 0; });
            EXPECT_EQ(subscriber->received_, message_value);
        }
    }
}

}  // namespace cris::core
