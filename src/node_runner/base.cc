#include "cris/core/node_runner/base.h"

#include "cris/core/logging.h"
#include "cris/core/node/base.h"

#include <cstddef>
#include <mutex>

namespace cris::core {

std::vector<CRMessageQueue*> CRNodeRunnerBase::GetNodeQueues(CRNodeBase* node) {
    return node->GetNodeQueues();
}

CRMultiThreadNodeRunner::CRMultiThreadNodeRunner(const std::vector<CRNodeBase*>& nodes, std::size_t thread_num)
    : CRNodeRunnerBase()
    , nodes_(nodes)
    , thread_num_(thread_num) {
    for (const auto* node : nodes_) {
        LOG(INFO) << __func__ << ": " << this << " initialized for " << node->GetName() << "(" << node << ")";
    }
    LOG(INFO) << __func__ << ": Runner " << this << "."
              << " Number of threads: " << thread_num_;
}

const std::vector<CRNodeBase*>& CRMultiThreadNodeRunner::GetNodes() const {
    return nodes_;
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
    CleanUp();
}

}  // namespace cris::core
