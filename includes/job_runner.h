#pragma once

#include <boost/lockfree/queue.hpp>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace cris::core {

class JobRunner {
   public:
    using Self = JobRunner;

    struct Config {
        std::size_t              thread_num_{1};
        std::size_t              always_active_thread_num_{0};
        std::chrono::nanoseconds active_time_{0};
    };

    explicit JobRunner(Config config);

    ~JobRunner();

    JobRunner(const Self&) = delete;
    JobRunner(Self&&)      = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&) = delete;

    using job_t = std::function<void()>;

    ///
    /// Add a job to run
    ///
    /// @param job            a function to run.
    /// @param scheduler_hint an integer used for determining the worker thread to run the job.
    ///                       Same integer means the job will assign to the same worker.
    ///                       Note that the jobs are stealable, so the worker that runs the job
    ///                       may be different from the assigned worker.
    void AddJob(job_t&& job, std::size_t scheduler_hint);

    void AddJob(job_t&& job) { return AddJob(std::move(job), DefaultSchedulerHint()); }

    ///
    /// Randomly steal a job from the workers and run.
    ///
    /// @return true if any job was stolen
    bool Steal();

    std::size_t ThreadNum() const;

    std::size_t ActiveThreadNum() const;

    std::size_t DefaultSchedulerHint();

   private:
    struct Worker {
        using job_queue_t = boost::lockfree::queue<job_t*>;

        explicit Worker(JobRunner* runner, std::size_t idx);

        ~Worker();

        bool TryProcessOne();

        void WorkerLoop();

        JobRunner*        runner_;
        std::size_t       index_;
        std::atomic<bool> shutdown_flag_{false};
        job_queue_t       job_queue_;
        std::thread       thread_;

        static constexpr std::size_t kInitialQueueCapacity = 8192;
    };

    Config                                     config_;
    std::atomic<bool>                          ready_for_stealing_{false};
    std::atomic<std::size_t>                   active_workers_num_{0};
    std::mutex                                 worker_inactive_mutex_;
    std::condition_variable                    worker_inactive_cv_;
    std::random_device                         random_device_;
    std::default_random_engine                 random_engine_;
    std::uniform_int_distribution<std::size_t> random_worker_selector_;
    std::vector<std::unique_ptr<Worker>>       workers_;

    static thread_local std::atomic<std::uintptr_t> kCurrentThreadJobRunner;
    static thread_local std::atomic<std::size_t>    kCurrentThreadWorkerIndex;
};

}  // namespace cris::core
