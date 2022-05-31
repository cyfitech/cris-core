#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <thread>
#include <vector>

namespace cris::core {

class JobRunnerWorker;

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

    void NotifyOneWorker();

    std::size_t ThreadNum() const;

    std::size_t ActiveThreadNum() const;

    std::size_t DefaultSchedulerHint();

   private:
    friend class JobRunnerWorker;

    using worker_list_t = std::vector<std::unique_ptr<JobRunnerWorker>>;

    Config                   config_;
    std::atomic<bool>        ready_for_stealing_{false};
    std::atomic<std::size_t> active_workers_num_{0};
    worker_list_t            workers_;
};

}  // namespace cris::core
