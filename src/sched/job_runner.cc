#include "cris/core/sched/job_runner.h"

#include "cris/core/sched/spin_impl.h"
#include "cris/core/sched/spin_mutex.h"
#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"
#include "cris/core/utils/time.h"

#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wambiguous-reversed-operator"
#endif

#include <boost/lockfree/queue.hpp>

#if defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <utility>

namespace cris::core {

static thread_local std::uintptr_t kCurrentThreadJobRunner   = 0;
static thread_local std::size_t    kCurrentThreadWorkerIndex = 0;

class JobRunnerWorker {
   public:
    using job_t       = JobRunner::job_t;
    using job_queue_t = boost::lockfree::queue<job_t*>;

    explicit JobRunnerWorker(JobRunner* runner, std::size_t idx);

    JobRunnerWorker(const JobRunnerWorker&) = delete;
    JobRunnerWorker(JobRunnerWorker&&)      = delete;
    JobRunnerWorker& operator=(const JobRunnerWorker&) = delete;
    JobRunnerWorker& operator=(JobRunnerWorker&&) = delete;

    ~JobRunnerWorker();

    std::unique_ptr<job_t> TryGetOneJob();

    bool TryProcessOne();

    void WorkerLoop();

    void Stop();

    void Join();

    // JobRunnerWorker is internal helpers for JobRunner, so their lifecycle are
    // always shorter than the matching runner, so raw pointer is OK here.
    JobRunner* runner_;

    std::size_t             index_;
    std::atomic<bool>       shutdown_flag_{false};
    std::atomic<bool>       stopped_flag_{false};
    std::mutex              inactive_cv_mutex_;
    std::condition_variable inactive_cv_;
    job_queue_t             job_queue_{kInitialQueueCapacity};
    std::thread             thread_;

    static constexpr std::size_t kInitialQueueCapacity = 8192;
};

class JobAliveToken {
   public:
    explicit JobAliveToken(std::weak_ptr<JobRunnerStrand> strand_weak) : strand_weak_(strand_weak) {}

    ~JobAliveToken();

    JobAliveToken(const JobAliveToken&) = delete;
    JobAliveToken(JobAliveToken&&)      = delete;
    JobAliveToken& operator=(const JobAliveToken&) = delete;
    JobAliveToken& operator=(JobAliveToken&&) = delete;

   private:
    std::weak_ptr<JobRunnerStrand> strand_weak_;
};

class JobRunnerStrand : public std::enable_shared_from_this<JobRunnerStrand> {
   public:
    using job_t       = JobRunner::job_t;
    using job_queue_t = boost::lockfree::queue<job_t*>;

    explicit JobRunnerStrand(std::weak_ptr<JobRunner> runner) : runner_weak_(runner) {}

    ~JobRunnerStrand();

    bool AddJob(job_t&& job);

    bool AddJob(std::function<void(JobAliveTokenPtr&&)>&& job);

    void PushToRunnerIfNeeded(const bool is_in_running_job);

   private:
    std::weak_ptr<JobRunner> runner_weak_;
    HybridSpinMutex          hybrid_spin_mtx_;
    bool                     has_ready_job_{false};
    job_queue_t              pending_jobs_{kInitialQueueCapacity};
    bool                     finished_{false};

    static constexpr std::size_t kInitialQueueCapacity = 8192;
};

JobAliveToken::~JobAliveToken() {
    // When the current job is finishing, try pushing the next one.
    if (auto strand = strand_weak_.lock()) {
        strand->PushToRunnerIfNeeded(/* is_in_running_job = */ true);
    }
}

JobRunnerStrand::~JobRunnerStrand() {
    finished_ = true;
    pending_jobs_.consume_all([](job_t* const job_ptr) { delete job_ptr; });
}

bool JobRunnerStrand::AddJob(JobRunnerStrand::job_t&& job) {
    return AddJob([job = std::move(job)](JobAliveTokenPtr&&) { job(); });
}

bool JobRunnerStrand::AddJob(std::function<void(JobAliveTokenPtr&&)>&& job) {
    if (finished_) {
        LOG(WARNING) << __func__ << ": JobRunnerStrand have finished.";
        return false;
    }

    auto serialized_job = [job         = std::move(job),
                           alive_token = std::make_shared<JobAliveToken>(weak_from_this())]() mutable {
        job(std::move(alive_token));
    };

    pending_jobs_.push(new job_t(std::move(serialized_job)));
    PushToRunnerIfNeeded(/* is_in_running_job = */ false);
    return true;
}

// Try to get the next job from the pending queue and run it.
//
// `is_in_running_job` indicates if this function is called at the end of the running job.
// Otherwise, it is called after a job is pushed to the pending queue. In either case,
// this function was called exactly once.
//
// This function guarantees the safety and liveness of the strand.
//
// Safety:
//   - There are no more than one jobs from the same strand in the matching runner;
//   - Jobs are added to the runner in the order of being pushed to the pending queue.
// Liveness:
//   - If there are jobs in the pending queue, there will be one of them being pushed
//     to the runner, even if there are no more incoming jobs.
//
// The function consists of 2 parts:
//   - Decision. It is protected by lock and it decides whether Push is needed.
//   - Push. It pushes the next job to the runner.
// Let's use + to denote `is_in_running_job == false` and `-` otherwise,
// use D to denote the D's that changed `has_ready_job_` value and d to those not, then
//   - D+ only changes the `has_ready_job_` value from false to true
//   - D- only changes the `has_ready_job_` value from true to false
//   - P and D/d- has one-to-one matching and P always happen before the correspoding D/d-
//   - Only D+ and d- generate P.
//   - Then the D/d who generates a P will be a sequence like this:
//         [D+ d-* (D-)]*
//     D- does not generate a P but we add it as a placeholder.
//   - Then we will prove:
//       - In this sequence, the P generated by a D/d will always happen before next D/d
//       - If there is some D/d, its P does not happen before the next D/d:
//           - If the next D/d is a D/d-, since it must be generated by some P, and all previous P's have a
//             matched D/d-, there are not enough P's to generate this one, so it is impossible.
//           - If the next D/d is a D+, then the current one is D-, but D- does not generate P.
//   - So the sequence is like the following:
//         D+ [P d-] [P d-] [P D-] D+ [P d-] P ...
//     Jobs are denoted by brackets. Safety proved.
//
//  For liveness, D/d+ add jobs, D+ will push job itself so liveness is trivially hold, for d-, the job will
//  be visible to the next D/d. If we looks at the non-d- decision before it, it can only be D+/d-, and both
//  of them comes with a next D/d. Liveness proved.
void JobRunnerStrand::PushToRunnerIfNeeded(const bool is_in_running_job) {
    if (finished_) {
        LOG(WARNING) << __func__ << ": JobRunnerStrand have finished.";
        return;
    }

    std::unique_ptr<job_t> next;

    // Decision(D/d). It is protected by lock and it decides whether Push is needed.
    //
    // `has_ready_job_` indicates if there is a job running in the runner.
    {
        std::unique_lock lock(hybrid_spin_mtx_);

        // Someone else is running or about to run a job.
        if (!is_in_running_job && has_ready_job_) {
            return;
        }

        pending_jobs_.consume_one([&next](job_t* const job_ptr) { next.reset(job_ptr); });

        if (!next) {
            has_ready_job_ = false;
            return;
        }
        has_ready_job_ = true;
    }

    // Push(P). It pushes the next job to the runner.
    if (auto runner = runner_weak_.lock()) {
        runner->AddJob(std::move(*next));
    }
}

JobRunner::JobRunner(JobRunner::Config config) : config_(config) {
    LOG(INFO) << __func__ << ": JobRunner at 0x" << std::hex << reinterpret_cast<std::uintptr_t>(this) << std::dec
              << " initialized with " << config_.thread_num_ << " worker(s). " << config_.always_active_thread_num_
              << " of them always stay active, others go to sleep if stay idle for more than "
              << std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(config_.active_time_).count()
              << " ms." << std::dec;
    workers_.reserve(config_.thread_num_);
    for (std::size_t idx = 0; idx < config_.thread_num_; ++idx) {
        workers_.push_back(std::make_unique<JobRunnerWorker>(this, idx));
    }
    ready_for_stealing_.store(true);
}

JobRunner::~JobRunner() {
    ready_for_stealing_.store(false);
    Stop();
}

JobRunnerStrandPtr JobRunner::MakeStrand() {
    return std::make_shared<JobRunnerStrand>(weak_from_this());
}

bool JobRunner::AddJob(job_t&& job, std::size_t scheduler_hint) {
    // In default case, modulo operation is not needed because the RNG guarantee the output range.
    // Use conditions to avoid unnecessary modulos.
    const std::size_t worker_idx =
        scheduler_hint < config_.thread_num_ ? scheduler_hint : scheduler_hint % config_.thread_num_;
    auto& worker = workers_[worker_idx];
    if (!worker) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Try to schedule to uninitialized worker " << worker_idx
                   << ", worker num: " << config_.thread_num_;
        return false;
    }

    worker->job_queue_.push(new job_t(std::move(job)));
    // Notify the scheduled worker first, so that it has better chance to
    // pick up this job.
    worker->inactive_cv_.notify_one();

    // Randomly notify another worker, in case all awake workers are busy,
    // and that worker may steal the jobs.
    if (!ready_for_stealing_.load()) {
        NotifyOneWorker();
    }
    return true;
}

bool JobRunner::AddJob(job_t&& job, JobRunnerStrandPtr strand) {
    return strand ? strand->AddJob(std::move(job)) : AddJob(std::move(job));
}

bool JobRunner::AddJob(std::function<void(JobAliveTokenPtr&&)>&& job, JobRunnerStrandPtr strand) {
    return strand ? strand->AddJob(std::move(job)) : AddJob(std::bind(std::move(job), nullptr));
}

bool JobRunner::Steal() {
    if (!ready_for_stealing_.load()) {
        return false;
    }

    static thread_local std::random_device         random_device;
    static thread_local std::default_random_engine random_engine(random_device());
    std::uniform_int_distribution<std::size_t>     random_worker_selector(0, config_.thread_num_);

    DLOG(INFO) << __func__ << ": JobRunnerWorker " << kCurrentThreadWorkerIndex << " stealing.";
    std::size_t idx = random_worker_selector(random_engine);
    for (std::size_t i = 0; i < config_.thread_num_; ++idx, ++i) {
        if (idx >= config_.thread_num_) {
            idx -= config_.thread_num_;
        }
        if (!workers_[idx]) [[unlikely]] {
            DLOG(FATAL) << __func__ << ": JobRunnerWorker " << idx << " is unexpectedly uninitialized.";
            continue;
        }
        if (workers_[idx]->TryProcessOne()) {
            DLOG(INFO) << __func__ << ": JobRunnerWorker " << kCurrentThreadWorkerIndex << " stole a job from " << idx
                       << ".";
            return true;
        }
    }
    return false;
}

void JobRunner::Stop() {
    for (auto& worker : workers_) {
        if (!worker) {
            continue;
        }
        worker->Stop();
        worker->Join();
    }
}

void JobRunner::NotifyOneWorker() {
    static thread_local std::random_device         random_device;
    static thread_local std::default_random_engine random_engine(random_device());
    std::uniform_int_distribution<std::size_t>     random_worker_selector(0, config_.thread_num_);

    std::size_t idx = random_worker_selector(random_engine);
    if (!workers_[idx]) [[unlikely]] {
        DLOG(FATAL) << __func__ << ": JobRunnerWorker " << idx << " is unexpectedly uninitialized.";
        return;
    }
    workers_[idx]->inactive_cv_.notify_one();
}

std::size_t JobRunner::ThreadNum() const {
    return config_.thread_num_;
}

std::size_t JobRunner::ActiveThreadNum() const {
    return active_workers_num_.load();
}

std::size_t JobRunner::DefaultSchedulerHint() {
    static thread_local std::random_device         random_device;
    static thread_local std::default_random_engine random_engine(random_device());
    std::uniform_int_distribution<std::size_t>     random_worker_selector(0, config_.thread_num_);

    return kCurrentThreadJobRunner == reinterpret_cast<std::uintptr_t>(this) ? kCurrentThreadWorkerIndex
                                                                             : random_worker_selector(random_engine);
}

std::shared_ptr<JobRunner> JobRunner::MakeJobRunner(Config config) {
    return std::shared_ptr<JobRunner>(new JobRunner(config));
}

JobRunnerWorker::JobRunnerWorker(JobRunner* runner, std::size_t idx)
    : runner_(runner)
    , index_(idx)
    , thread_(std::bind(&JobRunnerWorker::WorkerLoop, this)) {
}

JobRunnerWorker::~JobRunnerWorker() {
    DCHECK(!thread_.joinable());
    job_queue_.consume_all([](job_t* const job_ptr) { delete job_ptr; });
}

std::unique_ptr<JobRunner::job_t> JobRunnerWorker::TryGetOneJob() {
    std::unique_ptr<job_t> job{nullptr};
    job_queue_.consume_one([&job](job_t* const job_ptr) { job.reset(job_ptr); });
    return job;
}

bool JobRunnerWorker::TryProcessOne() {
    if (auto job = TryGetOneJob()) {
        (*job)();
        return true;
    }
    return false;
}

void JobRunnerWorker::WorkerLoop() {
    cr_timestamp_nsec_t last_active_timestamp = GetSystemTimestampNsec();

    const long long active_time_nsec =
        std::chrono::duration_cast<std::chrono::nanoseconds>(runner_->config_.active_time_).count();

    kCurrentThreadJobRunner   = reinterpret_cast<std::uintptr_t>(runner_);
    kCurrentThreadWorkerIndex = index_;

    bool has_pending_jobs = false;

    runner_->active_workers_num_.fetch_add(1);
    while (!shutdown_flag_.load()) {
        if (TryProcessOne() || runner_->Steal()) {
            has_pending_jobs = true;
            continue;
        }
        if (index_ < runner_->config_.always_active_thread_num_) {
            impl::SpinForApprox1us();
            continue;
        }
        if (has_pending_jobs) {
            last_active_timestamp = GetSystemTimestampNsec();
            has_pending_jobs      = false;
        }
        if (GetSystemTimestampNsec() < last_active_timestamp + active_time_nsec) {
            impl::SpinForApprox1us();
            continue;
        }
        runner_->active_workers_num_.fetch_sub(1);
        DLOG(INFO) << __func__ << ": JobRunnerWorker " << index_ << " is inactive.";
        std::unique_lock<std::mutex> lock(inactive_cv_mutex_);
        constexpr auto               kWorkerIdleTime = std::chrono::seconds(1);
        if (shutdown_flag_.load()) {
            break;
        }
        inactive_cv_.wait_for(lock, kWorkerIdleTime);
        DLOG(INFO) << __func__ << ": JobRunnerWorker " << index_ << " is active.";
        runner_->active_workers_num_.fetch_add(1);
    }
    runner_->active_workers_num_.fetch_sub(1);
    stopped_flag_.store(true);
}

void JobRunnerWorker::Stop() {
    bool                         expected_shutdown_flag = false;
    std::unique_lock<std::mutex> lock(inactive_cv_mutex_);
    if (!shutdown_flag_.compare_exchange_strong(expected_shutdown_flag, true)) {
        return;
    }
    inactive_cv_.notify_all();
    DLOG(INFO) << __func__ << ": JobRunnerWorker " << index_ << " is stopping.";
}

void JobRunnerWorker::Join() {
    if (!thread_.joinable()) {
        return;
    }
    DLOG(INFO) << __func__ << ": JobRunnerWorker " << index_ << " is stopped.";
    thread_.join();
    std::atomic_thread_fence(std::memory_order::seq_cst);
}

}  // namespace cris::core
