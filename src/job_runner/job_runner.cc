#include "cris/core/job_runner.h"

#include "cris/core/logging.h"
#include "cris/core/timer/timer.h"

#include <boost/lockfree/queue.hpp>

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

    ~JobRunnerWorker();

    std::unique_ptr<job_t> TryGetOneJob();

    bool TryProcessOne();

    void WorkerLoop();

    void Stop();

    void Join();

    JobRunner*              runner_;
    std::size_t             index_;
    std::atomic<bool>       shutdown_flag_{false};
    std::atomic<bool>       stopped_flag_{false};
    std::mutex              inactive_cv_mutex_;
    std::condition_variable inactive_cv_;
    job_queue_t             job_queue_{kInitialQueueCapacity};
    std::thread             thread_;

    static constexpr std::size_t kInitialQueueCapacity = 8192;
};

JobRunner::JobRunner(JobRunner::Config config) : config_(std::move(config)) {
    workers_.reserve(config_.thread_num_);
    for (std::size_t idx = 0; idx < config_.thread_num_; ++idx) {
        workers_.push_back(std::make_unique<JobRunnerWorker>(this, idx));
    }
    ready_for_stealing_.store(true);
}

JobRunner::~JobRunner() {
    ready_for_stealing_.store(false);
    for (auto& worker : workers_) {
        if (!worker) {
            continue;
        }
        worker->Stop();
        worker->Join();
    }
}

void JobRunner::AddJob(job_t&& job, std::size_t scheduler_hint) {
    // In default case, modulo operation is not needed because the RNG guarantee the output range.
    // Use conditions to avoid unnecessary modulos.
    const std::size_t worker_idx =
        scheduler_hint < config_.thread_num_ ? scheduler_hint : scheduler_hint % config_.thread_num_;
    auto& worker = workers_[worker_idx];
    if (!worker) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Try to schedule to uninitialized worker " << worker_idx
                   << ", worker num: " << config_.thread_num_;
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
}

bool JobRunner::Steal() {
    if (!ready_for_stealing_.load()) {
        return false;
    }

    static thread_local std::random_device         random_device;
    static thread_local std::default_random_engine random_engine(random_device());
    std::uniform_int_distribution<std::size_t> random_worker_selector(0, config_.thread_num_);

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

void JobRunner::NotifyOneWorker() {
    static thread_local std::random_device         random_device;
    static thread_local std::default_random_engine random_engine(random_device());
    std::uniform_int_distribution<std::size_t> random_worker_selector(0, config_.thread_num_);

    std::size_t idx = random_worker_selector(random_engine);
    if (!workers_[idx]) [[unlikely]] {
        DLOG(FATAL) << __func__ << ": JobRunnerWorker " << idx << " is unexpectedly uninitialized.";
        return;
    }
    workers_[idx]->inactive_cv_.notify_all();
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
    std::uniform_int_distribution<std::size_t> random_worker_selector(0, config_.thread_num_);

    return kCurrentThreadJobRunner == reinterpret_cast<std::uintptr_t>(this) ? kCurrentThreadWorkerIndex
                                                                             : random_worker_selector(random_engine);
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

    runner_->active_workers_num_.fetch_add(1);
    while (!shutdown_flag_.load()) {
        if (TryProcessOne() || runner_->Steal()) {
            last_active_timestamp = GetSystemTimestampNsec();
            continue;
        }
        if (index_ < runner_->config_.always_active_thread_num_) {
            continue;
        }
        if (GetSystemTimestampNsec() < last_active_timestamp + active_time_nsec) {
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
    bool expected_shutdown_flag = false;
    std::unique_lock<std::mutex> lock(inactive_cv_mutex_);
    if (!shutdown_flag_.compare_exchange_strong(expected_shutdown_flag, true)) {
        return;
    }
    inactive_cv_.notify_one();
    DLOG(INFO) << __func__ << ": JobRunnerWorker " << index_ << " is stopping.";
}

void JobRunnerWorker::Join() {
    DLOG(INFO) << __func__ << ": JobRunnerWorker " << index_ << " is stopped.";
    thread_.join();
    std::atomic_thread_fence(std::memory_order::seq_cst);
}

}  // namespace cris::core
