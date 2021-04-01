#include <thread>

#include "gtest/gtest.h"
#include "node/base.h"

namespace cris::core {

class TrivialNodeForTest : public CRNodeBase {
    CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) override { return nullptr; }

   private:
    void SubscribeImpl(std::string &&                                  message_name,
                       std::function<void(const CRMessageBasePtr &)> &&callback) override {}
};

TEST(NodeTest, WaitAndUnblock) {
    std::atomic<int>         unblocked{0};
    std::vector<std::thread> threads{};
    constexpr size_t         kThreadNum = 4;

    TrivialNodeForTest node;

    for (size_t i = 0; i < kThreadNum; ++i) {
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
    for (auto &&thread : threads) {
        thread.join();
    }
}

TEST(NodeTest, WaitAndTimeout) {
    std::atomic<int>         unblocked{0};
    std::vector<std::thread> threads{};
    constexpr size_t         kThreadNum = 4;

    TrivialNodeForTest node;

    for (size_t i = 0; i < kThreadNum; ++i) {
        threads.emplace_back([&unblocked, &node]() {
            node.WaitForMessage(std::chrono::seconds(1));
            unblocked.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(unblocked.load(), 0);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_EQ(unblocked.load(), kThreadNum);
    for (auto &&thread : threads) {
        thread.join();
    }
}

}  // namespace cris::core
