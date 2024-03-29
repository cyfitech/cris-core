#include "cris/core/sched/job_runner.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Use macros to keep line information.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage,-warnings-as-errors)
#define EVENTUALLY_EQ(lfs, rhs)                                   \
    do {                                                          \
        using namespace std::literals::chrono_literals;           \
        constexpr std::size_t kMaxAttempts = 1000;                \
        constexpr auto        kAttemptGap  = 5ms;                 \
        bool                  is_eq        = false;               \
        for (std::size_t _i = 0; _i < kMaxAttempts; ++_i) {       \
            if ((lfs) == (rhs)) {                                 \
                is_eq = true;                                     \
                break;                                            \
            }                                                     \
            std::this_thread::sleep_for(kAttemptGap);             \
        }                                                         \
        if (!is_eq) {                                             \
            /* Try a more time and print logs when mismatching */ \
            EXPECT_EQ(lfs, rhs);                                  \
        }                                                         \
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
    auto runner = JobRunner::MakeJobRunner(config);

    auto test_batch = [runner](const bool wait) {
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
            auto result = runner->AddJob(
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

        EXPECT_EQ(runner->ThreadNum(), kThreadNum);
        EVENTUALLY_EQ(runner->ActiveThreadNum(), 0);
    };

    for (std::size_t i = 0; i < kTestBatchNum; ++i) {
        test_batch(/* wait = */ true);
    }

    // These jobs may be discarded when the runner destructs.
    test_batch(/* wait = */ false);
}

TEST(JobRunnerTest, JobBatch) {
    static constexpr std::size_t kTestBatchNum  = 3;
    static constexpr std::size_t kTestBatchSize = 500;
    static constexpr std::size_t kThreadNum     = 4;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner = JobRunner::MakeJobRunner(config);

    for (std::size_t i = 0; i < kTestBatchNum; ++i) {
        std::mutex mtx;
        auto       cv = std::make_shared<std::condition_variable>();

        std::unique_lock lock(mtx);

        auto call_count = std::make_shared<std::atomic<std::size_t>>(0);

        std::vector<JobRunner::job_t> job_batch;
        job_batch.reserve(kTestBatchSize);

        for (std::size_t j = 0; j < kTestBatchSize; ++j) {
            job_batch.push_back([call_count, cv]() {
                call_count->fetch_add(1);
                cv->notify_all();
            });
        }

        EXPECT_TRUE(runner->AddJobs(std::move(job_batch)));
        EVENTUALLY_EQ(call_count->load(), kTestBatchSize);
    };
}

TEST(JobRunnerTest, JobLocality) {
    static constexpr std::size_t kThreadNum = 4;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner = JobRunner::MakeJobRunner(config);

    auto make_workers_busy = [runner]() {
        static constexpr auto kSingleJobDuration = std::chrono::milliseconds(200);
        for (std::size_t i = 0; i < kThreadNum; ++i) {
            // Assign them to different workers.
            EXPECT_TRUE(runner->AddJob([]() { std::this_thread::sleep_for(kSingleJobDuration); }, i));
        }
    };

    constexpr std::size_t kSpawningJobNum = kThreadNum;
    auto                  call_count      = std::make_shared<std::atomic<std::size_t>>(0);

    auto spawning_job = [runner, call_count]() {
        const auto thread_id = std::this_thread::get_id();

        auto result = runner->AddJob([thread_id, call_count]() {
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

    // Let the workers become busy.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    for (std::size_t i = 0; i < kSpawningJobNum; ++i) {
        // Assign them to different workers.
        EXPECT_TRUE(runner->AddJob(spawning_job, i));
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
    auto runner = JobRunner::MakeJobRunner(config);

    // Let the workers start.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(runner->ThreadNum(), kThreadNum);
    EXPECT_EQ(runner->ActiveThreadNum(), kThreadNum);

    // Within the active time, all threads are still active.
    std::this_thread::sleep_for(kActiveTime / 2);
    EXPECT_EQ(runner->ActiveThreadNum(), kThreadNum);

    // After the active time, only "always acitve" numbeggr of threads are active.
    EVENTUALLY_EQ(runner->ActiveThreadNum(), kAlwaysActiveThreadNum);
}

TEST(JobRunnerTest, StrandTest) {
    static constexpr std::size_t kThreadNum = 4;
    static constexpr std::size_t kJobNum    = 50000;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner = JobRunner::MakeJobRunner(config);
    auto strand = runner->MakeStrand();

    // Strand guarantees order, so no need to use lock/atomic variables.
    std::size_t expected_current_job_idx = 0;
    for (std::size_t job_idx = 0; job_idx < kJobNum; ++job_idx) {
        EXPECT_TRUE(runner->AddJob(
            [job_idx, &expected_current_job_idx]() { EXPECT_EQ(job_idx, expected_current_job_idx++); },
            strand));
    }

    // Use a different atomic flag for completion instead of the counter,
    // because TSAN might catch the data race on counter if we accidentally have concurrency on serialized jobs.
    std::atomic<bool> finish{false};
    EXPECT_TRUE(runner->AddJob([&finish]() { finish.store(true); }, strand));

    EVENTUALLY_EQ(finish.load(), true);
}

TEST(JobRunnerTest, JobAliveToken) {
    static constexpr std::size_t kThreadNum      = 4;
    static constexpr std::size_t kSpawningJobNum = 100;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner  = JobRunner::MakeJobRunner(config);
    auto strand  = runner->MakeStrand();
    auto counter = std::make_shared<std::atomic<std::size_t>>(0);

    EXPECT_TRUE(runner->AddJob(
        [runner, counter](JobAliveTokenPtr&& alive_token) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // Handing alive_token to the spawned jobs, so that the current job is not consider as finished
            // until all the spawned jobs exited, even if the current job exits immediately.
            for (std::size_t i = 0; i < kSpawningJobNum; ++i) {
                runner->AddJob([alive_token, counter] { counter->fetch_add(1); });
            }
        },
        strand));

    std::atomic<bool> finish{false};
    runner->AddJob(
        [counter, &finish]() {
            EXPECT_EQ(counter->load(), kSpawningJobNum);
            finish.store(true);
        },
        strand);
    EVENTUALLY_EQ(finish.load(), true);
}

TEST(JobRunnerTest, TryRunImmediatelyStrand) {
    static constexpr std::size_t kThreadNum = 4;
    static constexpr std::size_t kJobNum    = 50000;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner = JobRunner::MakeJobRunner(config);
    auto strand = runner->MakeStrand();

    // Strand guarantees order, so no need to use lock/atomic variables.
    std::size_t counter = 0;
    for (std::size_t i = 0; i < kJobNum; ++i) {
        const auto result = runner->AddJob([&counter] { ++counter; }, strand, JobRunner::TryRunImmediately());
        // The first one must run immediately because there are no dependencies.
        if (i == 0) {
            EXPECT_EQ(result, JobRunner::TryRunImmediately::State::FINISHED);
        } else {
            EXPECT_NE(result, JobRunner::TryRunImmediately::State::FAILED);
        }
    }

    std::atomic<bool> finish{false};
    runner->AddJob(
        [&counter, &finish]() {
            EXPECT_EQ(counter, kJobNum);
            finish.store(true);
        },
        strand);
    EVENTUALLY_EQ(finish.load(), true);
}

TEST(JobRunnerTest, TryRunImmediatelyNoStrand) {
    static constexpr std::size_t kThreadNum = 4;
    static constexpr std::size_t kJobNum    = 50000;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner = JobRunner::MakeJobRunner(config);

    std::atomic<std::size_t> counter = 0;
    for (std::size_t i = 0; i < kJobNum; ++i) {
        // No order restrictions, so all jobs must run immediately.
        const auto add_result = runner->AddJob(
            [&counter] { counter.fetch_add(1); },
            /* strand = */ nullptr,
            JobRunner::TryRunImmediately());
        EXPECT_EQ(add_result, JobRunner::TryRunImmediately::State::FINISHED);
    }
    EXPECT_EQ(counter.load(), kJobNum);
}

TEST(JobRunnerTest, TryRunImmediatelyStrandRecurse) {
    static constexpr std::size_t kThreadNum = 4;
    static constexpr std::size_t kJobNum    = 50000;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto              runner = JobRunner::MakeJobRunner(config);
    auto              strand = runner->MakeStrand();
    std::atomic<bool> finish{false};

    // Strand guarantees order, so no need to use lock/atomic variables.
    std::size_t counter       = 0;
    auto        recursive_job = [runner, strand, &counter, &finish](std::size_t idx, auto self) {
        if (idx >= kJobNum) {
            EXPECT_EQ(counter, kJobNum);
            finish.store(true);
            return;
        }
        EXPECT_EQ(counter, idx);
        ++counter;
        // Called within strand jobs, so the next job cannot run immediately.
        const auto add_result =
            runner->AddJob([self, idx] { self(idx + 1, self); }, strand, JobRunner::TryRunImmediately());
        EXPECT_EQ(add_result, JobRunner::TryRunImmediately::State::ENQUEUED);
    };

    // The first one must run immediately because there are no dependencies.
    const auto add_result =
        runner->AddJob([recursive_job] { recursive_job(0, recursive_job); }, strand, JobRunner::TryRunImmediately());
    EXPECT_EQ(add_result, JobRunner::TryRunImmediately::State::FINISHED);

    EVENTUALLY_EQ(finish.load(), true);
}

TEST(JobRunnerTest, TryRunImmediatelyDualStrand) {
    static constexpr std::size_t kThreadNum = 4;
    static constexpr std::size_t kJobNum    = 50000;

    JobRunner::Config config = {
        .thread_num_ = kThreadNum,
    };
    auto runner       = JobRunner::MakeJobRunner(config);
    auto outer_strand = runner->MakeStrand();
    auto inner_strand = runner->MakeStrand();

    // The outer jobs are serialized, so the inner jobs has no concurrency
    // and must be able to run immediately.
    for (std::size_t i = 0; i < kJobNum; ++i) {
        runner->AddJob(
            [runner, inner_strand] {
                const auto add_result = runner->AddJob([] {}, inner_strand, JobRunner::TryRunImmediately());
                EXPECT_EQ(add_result, JobRunner::TryRunImmediately::State::FINISHED);
            },
            outer_strand,
            JobRunner::TryRunImmediately());
    }

    std::atomic<bool> finish{false};
    runner->AddJob([&finish]() { finish.store(true); }, outer_strand);
    EVENTUALLY_EQ(finish.load(), true);
}

}  // namespace cris::core
