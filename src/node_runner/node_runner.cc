#include "cris/core/logging.h"
#include "cris/core/node_runner/runner.h"

namespace cris::core {

std::vector<CRMessageQueue*> CRNodeRunnerBase::GetNodeQueues() {
    return GetNode()->GetNodeQueues();
}

CRMultiThreadNodeRunner::CRMultiThreadNodeRunner(CRNodeBase* node, std::size_t thread_num)
    : CRNodeRunnerBase()
    , node_(node)
    , thread_num_(thread_num) {
    LOG(INFO) << __func__ << ": " << this << " initialized for " << node_->GetName() << "(" << node_ << "), "
              << "thread number: " << thread_num_;
}

CRMultiThreadNodeRunner::~CRMultiThreadNodeRunner() {
    Join();
}

CRNodeBase* CRMultiThreadNodeRunner::GetNode() const {
    return node_;
}

std::size_t CRMultiThreadNodeRunner::GetThreadNum() const {
    return thread_num_;
}

void CRMultiThreadNodeRunner::Run() {
    std::lock_guard<std::mutex>  state_lock(run_state_mutex_);
    std::unique_lock<std::mutex> threads_lock(run_threads_mutex_, std::try_to_lock);
    if (!threads_lock.owns_lock()) {
        LOG(ERROR) << __func__ << ": Failed to run, maybe others are running/joining. Runner: " << this;
        return;
    }

    if (is_running_) {
        LOG(WARNING) << __func__ << ": Runner " << this << " is currently running.";
        return;
    }
    is_running_ = true;

    LOG(INFO) << __func__ << ": runner " << this << " start running.";

    PrepareToRun();
    for (std::size_t i = 0; i < thread_num_; ++i) {
        worker_threads_.emplace_back(GetWorker(i, thread_num_));
    }
}

void CRMultiThreadNodeRunner::Stop() {
    std::lock_guard<std::mutex> lock(run_state_mutex_);
    if (!is_running_) {
        LOG(WARNING) << __func__ << ": Runner " << this << " is currently not running.";
        return;
    }
    is_running_ = false;
    NotifyWorkersToStop();

    LOG(INFO) << __func__ << ": runner " << this << " about to stop.";
}

void CRMultiThreadNodeRunner::Join() {
    std::lock_guard<std::mutex> lock(run_threads_mutex_);
    for (auto&& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();
}

}  // namespace cris::core
