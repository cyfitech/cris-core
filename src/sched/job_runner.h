#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace cris::core {

class JobRunnerWorker;

// An object for serialized jobs. The jobs bound to the same strand object must run sequentially.
class JobRunnerStrand;

// An object for extending the lifecycle of one job in the strand.
class JobAliveToken;

using JobRunnerStrandPtr = std::shared_ptr<JobRunnerStrand>;
using JobAliveTokenPtr   = std::shared_ptr<JobAliveToken>;

class JobRunner : public std::enable_shared_from_this<JobRunner> {
   public:
    using Self = JobRunner;

    struct Config {
        std::size_t              thread_num_{1};
        std::size_t              always_active_thread_num_{0};
        std::chrono::nanoseconds active_time_{0};
    };

    struct TryRunImmediately {
        enum class State {
            FAILED = 0,
            ENQUEUED,
            FINISHED,
        };
    };

    struct ForceRunImmediately {};

    ~JobRunner();

    JobRunner(const Self&)       = delete;
    JobRunner(Self&&)            = delete;
    Self& operator=(const Self&) = delete;
    Self& operator=(Self&&)      = delete;

    using job_t = std::function<void()>;

    [[nodiscard]] JobRunnerStrandPtr MakeStrand();

    ///
    /// Add a job to run
    ///
    /// @param job            a function to run.
    /// @param scheduler_hint an integer used for determining the worker thread to run the job.
    ///                       Same integer means the job will assign to the same worker.
    ///                       Note that the jobs are stealable, so the worker that runs the job
    ///                       may be different from the assigned worker.
    bool AddJob(job_t&& job, std::size_t scheduler_hint);

    bool AddJob(job_t&& job) { return AddJob(std::move(job), DefaultSchedulerHint()); }

    bool AddJob(job_t&& job, JobRunnerStrandPtr strand);

    bool AddJob(std::function<void(JobAliveTokenPtr&&)>&& job, JobRunnerStrandPtr strand);

    bool AddJobs(std::vector<job_t>&& jobs, std::size_t scheduler_hint);

    bool AddJobs(std::vector<job_t>&& jobs) { return AddJobs(std::move(jobs), DefaultSchedulerHint()); }

    ///
    /// Add a job to run, run the job in the current thread immediately if possible.
    ///
    /// @param job            a function to run.
    /// @param strand         strand for serializing jobs.
    ///
    /// @return               the state to indicate if the job is finished or queued.
    ///
    /// WARNING: It may extend the lifetime of the alive token of the caller until
    /// the completion of the callee, which may reduce concurrency and introduce
    /// performance regression. Also the call stack may increase.
    /// Make sure that you know what you are doing before using.
    TryRunImmediately::State AddJob(job_t&& job, JobRunnerStrandPtr strand, TryRunImmediately);

    TryRunImmediately::State AddJob(
        std::function<void(JobAliveTokenPtr&&)>&& job,
        JobRunnerStrandPtr                        strand,
        TryRunImmediately);

    bool AddJob(job_t job, JobRunnerStrandPtr strand, ForceRunImmediately);

    bool AddJob(std::function<void(JobAliveTokenPtr&&)> job, JobRunnerStrandPtr strand, ForceRunImmediately);

    ///
    /// Randomly steal a job from the workers and run.
    ///
    /// @return true if any job was stolen
    bool Steal();

    JobRunner& Stop();

    void Join();

    void NotifyOneWorker();

    std::size_t ThreadNum() const;

    std::size_t ActiveThreadNum() const;

    std::size_t DefaultSchedulerHint();

    static std::shared_ptr<JobRunner> MakeJobRunner(Config config);

   private:
    friend class JobRunnerWorker;

    using worker_list_t = std::vector<std::unique_ptr<JobRunnerWorker>>;

    explicit JobRunner(Config config);

    Config                   config_;
    std::atomic<bool>        ready_for_stealing_{false};
    std::atomic<std::size_t> active_workers_num_{0};
    worker_list_t            workers_;
};

}  // namespace cris::core
