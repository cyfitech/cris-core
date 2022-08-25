#include "cris/core/msg/node.h"

#include "gtest/gtest.h"

#include <array>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
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
        auto   runner = JobRunner::MakeJobRunner(JobRunner::Config{});
        CRNode subscriber(runner);

        auto cv_mtx   = std::make_shared<std::mutex>();
        auto cv       = std::make_shared<std::condition_variable>();
        auto received = std::make_shared<bool>(false);

        const int message_value = 0;

        // Different type, never called.
        subscriber.Subscribe<TestMessage<10086>>(channel_subid, [](const auto&) { FAIL(); });

        // Different subid, never called.
        subscriber.Subscribe<TestMessageType>(10010, [](const auto&) { FAIL(); });

        subscriber.Subscribe<TestMessageType>(
            channel_subid,
            [message_value, cv_mtx, cv, received](const std::shared_ptr<TestMessageType>& msg) {
                std::unique_lock cv_lck(*cv_mtx);
                EXPECT_EQ(msg->value_, message_value);
                *received = true;
                cv->notify_all();
            });

        // Duplicate subscription, first one wins.
        subscriber.Subscribe<TestMessageType>(channel_subid, [&](const auto&) { FAIL(); });

        publisher.Publish(channel_subid, std::make_shared<TestMessageType>(message_value));

        std::unique_lock cv_lck(*cv_mtx);
        cv->wait(cv_lck, [received]() { return *received; });
    }

    // Publishing while nobody subscribing.
    publisher.Publish(channel_subid, std::make_shared<TestMessageType>(1));
}

TEST(NodeTest, MultipleChannels) {
    constexpr std::size_t kNumOfMsgTypes   = 4;
    constexpr std::size_t kNumOfSubChannel = 4;

    auto   runner = JobRunner::MakeJobRunner(JobRunner::Config{});
    CRNode publisher;
    CRNode subscriber(runner);

    struct Received {
        std::array<std::array<int, kNumOfSubChannel>, kNumOfMsgTypes> data_{};
    };

    auto cv_mtx = std::make_shared<std::mutex>();
    auto cv     = std::make_shared<std::condition_variable>();

    auto received = std::make_shared<Received>();

    for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
        subscriber.Subscribe<TestMessage<0>>(
            channel_subid,
            [received, channel_subid, cv_mtx, cv](const std::shared_ptr<TestMessage<0>>& msg) {
                std::unique_lock cv_lck(*cv_mtx);
                received->data_[0][channel_subid] = msg->value_;
                cv->notify_all();
            });
        subscriber.Subscribe<TestMessage<1>>(
            channel_subid,
            [received, channel_subid, cv_mtx, cv](const std::shared_ptr<TestMessage<1>>& msg) {
                std::unique_lock cv_lck(*cv_mtx);
                received->data_[1][channel_subid] = msg->value_;
                cv->notify_all();
            });
        subscriber.Subscribe<TestMessage<2>>(
            channel_subid,
            [received, channel_subid, cv_mtx, cv](const std::shared_ptr<TestMessage<2>>& msg) {
                std::unique_lock cv_lck(*cv_mtx);
                received->data_[2][channel_subid] = msg->value_;
                cv->notify_all();
            });
        subscriber.Subscribe<TestMessage<3>>(
            channel_subid,
            [received, channel_subid, cv_mtx, cv](const std::shared_ptr<TestMessage<3>>& msg) {
                std::unique_lock cv_lck(*cv_mtx);
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

            std::unique_lock cv_lck(*cv_mtx);
            cv->wait(cv_lck, [received, channel_subid]() {
                return received->data_[message_type_idx][channel_subid] != 0;
            });

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], message_value);
        }

        {
            constexpr int message_type_idx = 1;
            const int     message_value    = static_cast<int>(channel_subid + 1) * 100 + message_type_idx;

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], 0);

            publisher.Publish(channel_subid, std::make_shared<TestMessage<message_type_idx>>(message_value));

            std::unique_lock cv_lck(*cv_mtx);
            cv->wait(cv_lck, [received, channel_subid]() {
                return received->data_[message_type_idx][channel_subid] != 0;
            });

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], message_value);
        }

        {
            constexpr int message_type_idx = 2;
            const int     message_value    = static_cast<int>(channel_subid + 1) * 100 + message_type_idx;

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], 0);

            publisher.Publish(channel_subid, std::make_shared<TestMessage<message_type_idx>>(message_value));

            std::unique_lock cv_lck(*cv_mtx);
            cv->wait(cv_lck, [received, channel_subid]() {
                return received->data_[message_type_idx][channel_subid] != 0;
            });

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], message_value);
        }

        {
            constexpr int message_type_idx = 3;
            const int     message_value    = static_cast<int>(channel_subid + 1) * 100 + message_type_idx;

            EXPECT_EQ(received->data_[message_type_idx][channel_subid], 0);

            publisher.Publish(channel_subid, std::make_shared<TestMessage<message_type_idx>>(message_value));

            std::unique_lock cv_lck(*cv_mtx);
            cv->wait(cv_lck, [received, channel_subid]() {
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

    auto runner = JobRunner::MakeJobRunner(JobRunner::Config{});

    auto cv_mtx = std::make_shared<std::mutex>();
    auto cv     = std::make_shared<std::condition_variable>();

    for (std::size_t i = 0; i < kNumOfSubscribers; ++i) {
        auto subscriber = std::make_unique<Subscriber>(runner);
        subscriber->Subscribe<TestMessageType>(
            channel_subid,
            [subscriber = subscriber.get(), cv_mtx, cv](const std::shared_ptr<TestMessageType>& message) {
                std::unique_lock cv_lck(*cv_mtx);
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
        std::unique_lock lck(*cv_mtx);

        for (auto& subscriber : subscribers) {
            cv->wait(lck, [subscriber = subscriber.get()]() { return subscriber->received_ != 0; });
            EXPECT_EQ(subscriber->received_, message_value);
        }
    }

    {
        const int message_value = 200;

        for (auto& subscriber : subscribers) {
            subscriber->received_ = 0;
        }
        publisher.Publish(channel_subid, std::make_shared<TestMessageType>(message_value));
        std::unique_lock lck(*cv_mtx);

        for (auto& subscriber : subscribers) {
            cv->wait(lck, [subscriber = subscriber.get()]() { return subscriber->received_ != 0; });
            EXPECT_EQ(subscriber->received_, message_value);
        }
    }
}

TEST(NodeTest, StrandSubscriber) {
    static constexpr std::size_t kThreadNum       = 4;
    constexpr std::size_t        kNumOfSubChannel = 4;
    constexpr std::size_t        kMessageNumber   = 50000;

    using TestMessageType = TestMessage<11>;

    auto runner = JobRunner::MakeJobRunner(JobRunner::Config{
        .thread_num_ = kThreadNum,
    });

    CRNode publisher;
    CRNode subscriber(runner);

    std::mutex                                cv_mtx;
    std::condition_variable                   cv;
    std::array<std::size_t, kNumOfSubChannel> channel_counter{};
    std::array<bool, kNumOfSubChannel>        complete{};

    for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
        subscriber.Subscribe<TestMessageType>(
            channel_subid,
            [&counter = channel_counter[channel_subid], &complete = complete[channel_subid], &cv_mtx, &cv](
                const std::shared_ptr<TestMessageType>& message) {
                EXPECT_EQ(counter++, message->value_);
                if (counter == kMessageNumber) {
                    std::unique_lock cv_lck(cv_mtx);
                    complete = true;
                    cv.notify_all();
                }
            },
            /* allow_concurrency = */ false);
    }

    for (std::size_t msg_idx = 0; msg_idx < kMessageNumber; ++msg_idx) {
        for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
            publisher.Publish(channel_subid, std::make_shared<TestMessageType>(msg_idx));
        }
    }

    std::unique_lock cv_lck(cv_mtx);
    cv.wait(cv_lck, [&complete]() {
        return std::reduce(complete.begin(), complete.end(), true, std::logical_and<void>());
    });
}

TEST(NodeTest, JobAliveToken) {
    static constexpr std::size_t kThreadNum       = 4;
    constexpr std::size_t        kNumOfSubChannel = 4;
    constexpr std::size_t        kMessageNumber   = 50000;

    using TestMessageType = TestMessage<11>;

    auto runner = JobRunner::MakeJobRunner(JobRunner::Config{
        .thread_num_ = kThreadNum,
    });

    CRNode publisher;
    CRNode subscriber(runner);

    std::mutex                                cv_mtx;
    std::condition_variable                   cv;
    std::array<std::size_t, kNumOfSubChannel> channel_counter{};
    std::array<bool, kNumOfSubChannel>        complete{};

    for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
        subscriber.Subscribe<TestMessageType>(
            channel_subid,
            [&counter = channel_counter[channel_subid], &complete = complete[channel_subid], &cv_mtx, &cv, runner](
                const std::shared_ptr<TestMessageType>& message,
                JobAliveTokenPtr&&                      token) {
                ++counter;
                runner->AddJob([&counter, &complete, &cv_mtx, &cv, message, token] {
                    EXPECT_EQ(counter - 1, message->value_);
                    if (counter == kMessageNumber) {
                        std::unique_lock cv_lck(cv_mtx);
                        complete = true;
                        cv.notify_all();
                    }
                });
            },
            /* allow_concurrency = */ false);
    }

    for (std::size_t msg_idx = 0; msg_idx < kMessageNumber; ++msg_idx) {
        for (std::size_t channel_subid = 0; channel_subid < kNumOfSubChannel; ++channel_subid) {
            publisher.Publish(channel_subid, std::make_shared<TestMessageType>(msg_idx));
        }
    }

    std::unique_lock cv_lck(cv_mtx);
    cv.wait(cv_lck, [&complete]() {
        return std::reduce(complete.begin(), complete.end(), true, std::logical_and<void>());
    });
}

}  // namespace cris::core
