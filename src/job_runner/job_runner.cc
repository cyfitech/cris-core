#include "cris/core/job_runner.h"

#include "cris/core/logging.h"
#include "cris/core/timer/timer.h"

#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace cris::core {

thread_local std::atomic<std::uintptr_t> JobRunner::kCurrentThreadJobRunner   = 0;
thread_local std::atomic<std::size_t>    JobRunner::kCurrentThreadWorkerIndex = 0;

thread_local std::random_device         JobRunner::random_device;
thread_local std::default_random_engine JobRunner::random_engine(random_device());

JobRunner::JobRunner(JobRunner::Config config) : config_(std::move(config)) {
    workers_.reserve(config_.thread_num_);
    for (std::size_t idx = 0; idx < config_.thread_num_; ++idx) {
        workers_.push_back(std::make_unique<Worker>(this, idx));
    }
    ready_for_stealing_.store(true);
}

JobRunner::~JobRunner() {
    ready_for_stealing_.store(false);
    for (auto&& worker : workers_) {
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
    worker->inactive_cv_.notify_all();

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

    std::uniform_int_distribution<std::size_t> random_worker_selector(0, config_.thread_num_);

    DLOG(INFO) << __func__ << ": Worker " << kCurrentThreadWorkerIndex.load() << " stealing.";
    std::size_t idx = random_worker_selector(random_engine);
    for (std::size_t i = 0; i < config_.thread_num_; ++idx, ++i) {
        if (idx >= config_.thread_num_) {
            idx -= config_.thread_num_;
        }
        if (!workers_[idx]) [[unlikely]] {
            DLOG(FATAL) << __func__ << ": Worker " << idx << " is unexpectedly uninitialized.";
            continue;
        }
        if (workers_[idx]->TryProcessOne()) {
            DLOG(INFO) << __func__ << ": Worker " << kCurrentThreadWorkerIndex.load() << " stole a job from " << idx
                       << ".";
            return true;
        }
    }
    return false;
}

void JobRunner::NotifyOneWorker() {
    std::uniform_int_distribution<std::size_t> random_worker_selector(0, config_.thread_num_);

    std::size_t idx = random_worker_selector(random_engine);
    if (!workers_[idx]) [[unlikely]] {
        DLOG(FATAL) << __func__ << ": Worker " << idx << " is unexpectedly uninitialized.";
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
    std::uniform_int_distribution<std::size_t> random_worker_selector(0, config_.thread_num_);

    return kCurrentThreadJobRunner.load() == reinterpret_cast<std::uintptr_t>(this)
        ? kCurrentThreadWorkerIndex.load()
        : random_worker_selector(random_engine);
}

JobRunner::Worker::Worker(JobRunner* runner, std::size_t idx)
    : runner_(runner)
    , index_(idx)
    , thread_(std::bind(&Worker::WorkerLoop, this)) {
}

JobRunner::Worker::~Worker() {
    Stop();
    Join();
    job_queue_.consume_all([](job_t* const job_ptr) { delete job_ptr; });
}

std::unique_ptr<JobRunner::job_t> JobRunner::Worker::TryGetOneJob() {
    std::unique_ptr<job_t> job{nullptr};
    job_queue_.consume_one([&job](job_t* const job_ptr) { job.reset(job_ptr); });
    return job;
}

bool JobRunner::Worker::TryProcessOne() {
    if (auto job = TryGetOneJob()) {
        (*job)();
        return true;
    }
    return false;
}

void JobRunner::Worker::WorkerLoop() {
    cr_timestamp_nsec_t last_active_timestamp = GetSystemTimestampNsec();

    const long long active_time_nsec =
        std::chrono::duration_cast<std::chrono::nanoseconds>(runner_->config_.active_time_).count();

    kCurrentThreadJobRunner.store(reinterpret_cast<std::uintptr_t>(runner_));
    kCurrentThreadWorkerIndex.store(index_);

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
        DLOG(INFO) << __func__ << ": Worker " << index_ << " is inactive.";
        std::unique_lock<std::mutex> lock(inactive_cv_mutex_);
        constexpr auto               kWorkerIdleTime = std::chrono::seconds(1);
        inactive_cv_.wait_for(lock, kWorkerIdleTime);
        DLOG(INFO) << __func__ << ": Worker " << index_ << " is active.";
        runner_->active_workers_num_.fetch_add(1);
    }

    runner_->active_workers_num_.fetch_sub(1);
}

void JobRunner::Worker::Stop() {
    shutdown_flag_.store(true);
    DLOG(INFO) << __func__ << ": Worker " << index_ << " is stopping.";
    inactive_cv_.notify_all();
}

void JobRunner::Worker::Join() {
    if (thread_.joinable()) {
        thread_.join();
    }
    CHECK(!thread_.joinable());
    DLOG(INFO) << __func__ << ": Worker " << index_ << " is stopped.";
}

}  // namespace cris::core
