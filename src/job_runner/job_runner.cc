#include "cris/core/job_runner/job_runner.h"

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

JobRunner::JobRunner(JobRunner::Config config)
    : config_(std::move(config))
    , random_engine_(random_device_())
    , random_worker_selector_(0, config_.thread_num_ - 1) {
    workers_.reserve(config_.thread_num_);
    for (std::size_t idx = 0; idx < config_.thread_num_; ++idx) {
        workers_.push_back(std::make_unique<Worker>(this, idx));
    }
    for (auto&& worker : workers_) {
        worker->Start();
    }
}

JobRunner::~JobRunner() {
    for (auto&& worker : workers_) {
        worker->Stop();
        worker->Join();
    }
}

void JobRunner::AddJob(job_t&& job, std::size_t scheduler_hint) {
    auto& worker = *workers_[scheduler_hint % workers_.size()];
    worker.job_queue_.push(new job_t(std::move(job)));
    worker_inactive_cv_.notify_one();
}

bool JobRunner::Steal() {
    DLOG(INFO) << __func__ << ": Worker " << kCurrentThreadWorkerIndex.load() << " stealing.";
    std::size_t idx = random_worker_selector_(random_engine_);
    for (std::size_t i = 0; i < workers_.size(); ++idx, ++i) {
        if (idx >= workers_.size()) {
            idx -= workers_.size();
        }
        if (workers_[idx]->TryProcessOne()) {
            DLOG(INFO) << __func__ << ": Worker " << kCurrentThreadWorkerIndex.load() << " stole a job from " << idx
                       << ".";
            return true;
        }
    }
    return false;
}

std::size_t JobRunner::ThreadNum() const {
    return config_.thread_num_;
}

std::size_t JobRunner::ActiveThreadNum() const {
    return active_workers_num_.load();
}

std::size_t JobRunner::DefaultSchedulerHint() {
    return kCurrentThreadJobRunner.load() == reinterpret_cast<std::uintptr_t>(this)
        ? kCurrentThreadWorkerIndex.load()
        : random_worker_selector_(random_engine_);
}

JobRunner::Worker::Worker(JobRunner* runner, std::size_t idx)
    : runner_(runner)
    , index_(idx)
    , job_queue_(kInitialQueueCapacity) {
}

JobRunner::Worker::~Worker() {
    Stop();
    Join();
    job_queue_.consume_all([](job_t* job) { delete job; });
}

bool JobRunner::Worker::TryProcessOne() {
    return job_queue_.consume_one([](job_t* job) {
        (*job)();
        delete job;
    });
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
        std::unique_lock<std::mutex> lock(runner_->worker_inactive_mutex_);
        constexpr auto               kWorkerIdleTime = std::chrono::seconds(1);
        runner_->worker_inactive_cv_.wait_for(lock, kWorkerIdleTime);
        DLOG(INFO) << __func__ << ": Worker " << index_ << " is active.";
        runner_->active_workers_num_.fetch_add(1);
    }

    runner_->active_workers_num_.fetch_sub(1);
}

void JobRunner::Worker::Start() {
    if (thread_.joinable()) {
        return;
    }
    thread_ = std::thread(std::bind(&Worker::WorkerLoop, this));
}

void JobRunner::Worker::Stop() {
    shutdown_flag_.store(true);
    DLOG(INFO) << __func__ << ": Worker " << index_ << " is stopping.";
    runner_->worker_inactive_cv_.notify_all();
}

void JobRunner::Worker::Join() {
    if (thread_.joinable()) {
        thread_.join();
    }
    DLOG(INFO) << __func__ << ": Worker " << index_ << " is stopped.";
}

}  // namespace cris::core
