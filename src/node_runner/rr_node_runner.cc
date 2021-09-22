#include "cris/core/node_runner/runner.h"

#include <glog/logging.h>

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
    while (!mShutdownFlag.load()) {
        bool hasMessage = false;
        for (size_t i = thread_idx; i < mNodeQueues.size(); i += thread_num) {
            auto queue = mNodeQueues[i];
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
    mShutdownFlag.store(false);
    mNodeQueues = GetNodeQueues();
}

std::function<void()> CRNodeRoundRobinQueueProcessor::GetWorker(size_t thread_idx, size_t thread_num) {
    return std::bind(&CRNodeRoundRobinQueueProcessor::WorkerLoop, this, thread_idx, thread_num);
}

void CRNodeRoundRobinQueueProcessor::NotifyWorkersToStop() {
    mShutdownFlag.store(true);
}

}  // namespace cris::core
