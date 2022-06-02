#include "cris/core/job_runner.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#define EVENTUALLY_EQ(lfs, rhs)                                            \
    do {                                                                   \
        constexpr std::size_t kMaxAttempts = 1000;                         \
        constexpr auto        kAttemptGap  = std::chrono::milliseconds(5); \
        bool                  is_eq        = false;                        \
        for (std::size_t i = 0; i < kMaxAttempts; ++i) {                   \
            if ((lfs) == (rhs)) {                                          \
                is_eq = true;                                              \
                break;                                                     \
            }                                                              \
            std::this_thread::sleep_for(kAttemptGap);                      \
        }                                                                  \
        if (!is_eq) {                                                      \
            /* Try a more time and print logs when mismatching */          \
            EXPECT_EQ(lfs, rhs);                                           \
        }                                                                  \
    } while (0)

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
        std::mutex mtx;
        auto       cv = std::make_shared<std::condition_variable>();

        std::unique_lock lock(mtx);

        auto call_count = std::make_shared<std::atomic<std::size_t>>(0);

        using worker_thread_id_record_t     = std::atomic<std::thread::id>;
        using worker_thread_id_record_ptr_t = std::unique_ptr<worker_thread_id_record_t>;

        auto worker_thread_ids = std::make_shared<std::vector<worker_thread_id_record_ptr_t>>();

        for (std::size_t i = 0; i < kJobNum; ++i) {
            worker_thread_ids->push_back(std::make_unique<worker_thread_id_record_t>());
        }

        for (std::size_t i = 0; i < kJobNum; ++i) {
            // Schedule the jobs to the same worker first, let the runner itself
            // do the load-balancing.
            auto result = runner.AddJob(
                [call_count, cv, worker_thread_ids, i]() {
                    std::this_thread::sleep_for(kSingleJobDuration);
                    (*worker_thread_ids)[i]->store(std::this_thread::get_id());
                    call_count->fetch_add(1);
                    cv->notify_all();
                },
                0);
            EXPECT_TRUE(result);
        }

        if (!wait) {
            return;
        }

        while (call_count->load() < kJobNum) {
            cv->wait_for(lock, std::chrono::milliseconds(100));
        }

        std::set<std::thread::id> scheduled_worker_threads;
        for (auto&& worker_thread_id : *worker_thread_ids) {
            scheduled_worker_threads.insert(worker_thread_id->load());
        }

        // All jobs are executed.
        EXPECT_EQ(call_count->load(), kJobNum);

        // All workers had jobs.
        EXPECT_EQ(scheduled_worker_threads.size(), kThreadNum);

        EXPECT_EQ(runner.ThreadNum(), kThreadNum);
        EVENTUALLY_EQ(runner.ActiveThreadNum(), 0);
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
            EXPECT_TRUE(runner.AddJob([]() { std::this_thread::sleep_for(kSingleJobDuration); }, i));
        }
    };

    constexpr std::size_t kSpawningJobNum = kThreadNum;
    auto                  call_count      = std::make_shared<std::atomic<std::size_t>>(0);

    auto spawning_job = [&runner, call_count]() {
        const auto thread_id = std::this_thread::get_id();

        auto result = runner.AddJob([thread_id, call_count]() {
            call_count->fetch_add(1);
            // When no other idle workers, by default the spawned job will be run on the same
            // worker as the spawner was.
            EXPECT_EQ(std::this_thread::get_id(), thread_id);

            // Keep the current worker busy for a while.
            std::this_thread::sleep_for(std::chrono::seconds(1));
        });
        EXPECT_TRUE(result);
    };

    make_workers_busy();

    // Let the workers become busy
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (std::size_t i = 0; i < kSpawningJobNum; ++i) {
        // Assign them to different workers
        EXPECT_TRUE(runner.AddJob(spawning_job, i));
    }

    EVENTUALLY_EQ(call_count->load(), kSpawningJobNum);
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
    EVENTUALLY_EQ(runner.ActiveThreadNum(), kAlwaysActiveThreadNum);
}

}  // namespace cris::core
