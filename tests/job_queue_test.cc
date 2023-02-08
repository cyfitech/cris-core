#include "cris/core/sched/job_lock_queue.h"
#include "cris/core/sched/job_lockfree_queue.h"

#include "gtest/gtest.h"

#include <cstddef>
#include <memory>
#include <thread>

namespace cris::core {

void JobQueueTest(std::unique_ptr<JobQueue>&& queue) {
    EXPECT_TRUE(queue->Empty());

    for (int i = 0; i < 100; ++i) {
        int tmp = 0;
        queue->Push([&tmp, i]() { tmp = i; });
        EXPECT_TRUE(queue->ConsumeOne([](auto&& job) { job(); }));
        EXPECT_EQ(tmp, i);
        EXPECT_FALSE(queue->ConsumeOne([](auto&&) {}));
        EXPECT_FALSE(queue->ConsumeOne([](auto&&) {}));
    }
    EXPECT_TRUE(queue->Empty());

    {
        constexpr std::size_t kJobNum = 3415;

        // Notice that there are no concurrency on this temporary variable.
        // Both producer and consumer writes to the queue, but only consumer
        // is accessing this variable.
        std::size_t tmp = 0;

        std::thread producer([&queue, &tmp]() {
            for (std::size_t i = 0; i < kJobNum; ++i) {
                queue->Push([i, &tmp]() { tmp = i; });
            }
        });

        {
            // consumer
            for (std::size_t i = 0; i < kJobNum;) {
                if (!queue->ConsumeOne([](auto&& job) { job(); })) {
                    continue;
                }
                EXPECT_EQ(tmp, i++);
            }
        }

        producer.join();
        EXPECT_TRUE(queue->Empty());
    }

    {
        constexpr std::size_t kJobBatchNum  = 78925;
        constexpr std::size_t kJobBatchSize = 13;

        // Notice that there are no concurrency on this temporary variable.
        // Both producer and consumer writes to the queue, but only consumer
        // is accessing this variable.
        std::size_t tmp = 0;

        std::thread producer([&queue, &tmp]() {
            for (std::size_t i = 0; i < kJobBatchNum; ++i) {
                std::vector<JobQueue::job_t> job_batch;
                job_batch.reserve(kJobBatchSize);
                for (std::size_t j = 0; j < kJobBatchSize; ++j) {
                    job_batch.push_back([i, j, &tmp]() { tmp = i * kJobBatchSize + j; });
                }

                queue->PushBatch(std::move(job_batch));
            }
        });

        {
            // consumer
            for (std::size_t i = 0; i < kJobBatchNum * kJobBatchSize;) {
                if (!queue->ConsumeAll([&i, &tmp](auto&& job) {
                        job();
                        EXPECT_EQ(tmp, i++);
                    })) {
                    continue;
                }
            }
            EXPECT_EQ(tmp, kJobBatchNum * kJobBatchSize - 1);
        }

        producer.join();
    }
}

// Keep the initial capacity low, so that resize will be triggered during the tests.
constexpr std::size_t kInitQueueCapacity = 8;

TEST(JobQueueTest, LockQueue) {
    JobQueueTest(std::make_unique<JobLockQueue>(kInitQueueCapacity));
}

TEST(JobQueueTest, LockFreeQueue) {
    JobQueueTest(std::make_unique<JobLockFreeQueue>(kInitQueueCapacity));
}

}  // namespace cris::core
