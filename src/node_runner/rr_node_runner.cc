#include <glog/logging.h>

#include "cris/core/node_runner/runner.h"

namespace cris::core {

using std::literals::chrono_literals::operator""ms;

static constexpr auto kWaitMessageTimeout = 500ms;

CRNodeRoundRobinQueueProcessor::CRNodeRoundRobinQueueProcessor(CRNodeBase* node, size_t thread_num, bool auto_run)
    : CRMultiThreadNodeRunner(node, thread_num) {
    LOG(INFO) << __func__ << ": " << this << " initialized";
    if (auto_run) {
        Run();
    }
}

CRNodeRoundRobinQueueProcessor::~CRNodeRoundRobinQueueProcessor() {
    Stop();
    Join();
}

void CRNodeRoundRobinQueueProcessor::WorkerLoop(const size_t thread_idx, const size_t thread_num) {
    while (!shutdown_flag_.load()) {
        bool hasMessage = false;
        for (size_t i = thread_idx; i < node_queues_.size(); i += thread_num) {
            auto queue = node_queues_[i];
            if (queue->IsEmpty()) {
                continue;
            }
            hasMessage = true;
            queue->PopAndProcess(false);
        }
        if (!hasMessage) {
            GetNode()->WaitForMessage(kWaitMessageTimeout);
        }
    }
}

void CRNodeRoundRobinQueueProcessor::PrepareToRun() {
    shutdown_flag_.store(false);
    node_queues_ = GetNodeQueues();
}

std::function<void()> CRNodeRoundRobinQueueProcessor::GetWorker(size_t thread_idx, size_t thread_num) {
    return std::bind(&CRNodeRoundRobinQueueProcessor::WorkerLoop, this, thread_idx, thread_num);
}

void CRNodeRoundRobinQueueProcessor::NotifyWorkersToStop() {
    shutdown_flag_.store(true);
}

}  // namespace cris::core
