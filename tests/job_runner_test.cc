#include "cris/core/job_runner.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

namespace cris::core {

TEST(JobRunnerTest, Basic) {
    static constexpr std::size_t kTestBatchNum      = 3;
    static constexpr std::size_t kThreadNum         = 4;
    static constexpr std::size_t kJobNum            = 500;
    static constexpr auto        kSingleJobDuration = std::chrono::milliseconds(10);

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    JobRunner runner(config);

    auto test_batch = [&runner](const bool wait) {
        std::mutex              mtx;
        std::condition_variable cv;
        std::unique_lock        lock(mtx);

        std::atomic<std::size_t> call_count{0};
        std::vector<std::thread::id> worker_thread_ids(kJobNum);

        for (std::size_t i = 0; i < kJobNum; ++i) {
            // Schedule the jobs to the same worker first, let the runner itself
            // do the load-balancing.
            runner.AddJob(
                [&call_count, &cv, &worker_thread_id = worker_thread_ids[i]]() {
                    std::this_thread::sleep_for(kSingleJobDuration);
                    worker_thread_id = std::this_thread::get_id();
                    call_count.fetch_add(1);
                    cv.notify_all();
                },
                0);
        }

        if (!wait) {
            return;
        }

        while (call_count.load() < kJobNum) {
            cv.wait(lock);
        }

        std::set<std::thread::id> scheduled_worker_threads;
        for (auto&& worker_thread_id : worker_thread_ids) {
            scheduled_worker_threads.insert(worker_thread_id);
        }

        // All jobs are executed.
        EXPECT_EQ(call_count.load(), kJobNum);

        // All workers had jobs.
        EXPECT_EQ(scheduled_worker_threads.size(), kThreadNum);

        EXPECT_EQ(runner.ThreadNum(), kThreadNum);
        EXPECT_EQ(runner.ActiveThreadNum(), 0);
    };

    for (std::size_t i = 0; i < kTestBatchNum; ++i) {
        test_batch(/* wait = */ true);
    }

    // These jobs may be discarded when the runner destructs.
    test_batch(/* wait = */ false);
}

TEST(JobRunnerTest, JobLocality) {
    static constexpr std::size_t kThreadNum = 4;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    JobRunner runner(config);

    auto make_workers_busy = [&runner]() {
        static constexpr auto kSingleJobDuration = std::chrono::milliseconds(200);
        for (std::size_t i = 0; i < kThreadNum; ++i) {
            // Assign them to different workers
            runner.AddJob([]() { std::this_thread::sleep_for(kSingleJobDuration); }, i);
        }
    };

    constexpr std::size_t    kSpawningJobNum = kThreadNum;
    std::atomic<std::size_t> call_count{0};

    auto spawning_job = [&runner, &call_count]() {
        const auto thread_id = std::this_thread::get_id();

        runner.AddJob([thread_id, &call_count]() {
            call_count.fetch_add(1);
            // When no other idle workers, by default the spawned job will be run on the same
            // worker as the spawner was.
            EXPECT_EQ(std::this_thread::get_id(), thread_id);

            // Keep the current worker busy for a while.
            std::this_thread::sleep_for(std::chrono::seconds(1));
        });
    };

    make_workers_busy();

    // Let the workers become busy
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (std::size_t i = 0; i < kSpawningJobNum; ++i) {
        // Assign them to different workers
        runner.AddJob(spawning_job, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(call_count.load(), kSpawningJobNum);
}

TEST(JobRunnerTest, AlwaysActiveThread) {
    constexpr std::size_t kThreadNum             = 4;
    constexpr std::size_t kAlwaysActiveThreadNum = 2;
    constexpr auto        kActiveTime            = std::chrono::milliseconds(500);

    JobRunner::Config config = {
        .thread_num_               = kThreadNum,
        .always_active_thread_num_ = kAlwaysActiveThreadNum,
        .active_time_              = kActiveTime,
    };
    JobRunner runner(config);

    // Let the workers start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(runner.ThreadNum(), kThreadNum);
    EXPECT_EQ(runner.ActiveThreadNum(), kThreadNum);

    // Within the active time, all threads are still active.
    std::this_thread::sleep_for(kActiveTime / 2);
    EXPECT_EQ(runner.ActiveThreadNum(), kThreadNum);

    // After the active time, only "always acitve" number of threads are active.
    std::this_thread::sleep_for(kActiveTime);
    EXPECT_EQ(runner.ActiveThreadNum(), kAlwaysActiveThreadNum);
}

}  // namespace cris::core
