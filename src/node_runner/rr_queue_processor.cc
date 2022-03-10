#include "cris/core/node_runner/rr_queue_processor.h"

#include "cris/core/logging.h"

#include <chrono>
#include <cstddef>
#include <cstdlib>

namespace cris::core {

using std::literals::chrono_literals::operator""ms;

static constexpr auto kWaitMessageTimeout = 500ms;

CRNodeRoundRobinQueueProcessor::CRNodeRoundRobinQueueProcessor(
    const std::vector<CRNodeBase*>& nodes,
    std::size_t                     thread_num)
    : CRNodeQueueProcessor(nodes, thread_num) {
    LOG(INFO) << __func__ << ": " << this << " initialized";
}

CRNodeRoundRobinQueueProcessor::~CRNodeRoundRobinQueueProcessor() {
    Stop();
    Join();
}

bool CRNodeRoundRobinQueueProcessor::TryProcessQueues(
    const std::size_t start_index,
    const std::size_t stride,
    const bool        only_one) {
    bool has_message = false;
    for (std::size_t i = start_index; i < node_queues_.size(); i += stride) {
        auto queue = node_queues_[i];
        if (queue->IsEmpty()) {
            continue;
        }
        has_message = true;
        queue->PopAndProcess(false);
        if (only_one) {
            break;
        }
    }
    return has_message;
}

void CRNodeRoundRobinQueueProcessor::WorkerLoop(const std::size_t thread_idx, const std::size_t thread_num) {
    constexpr int kBusyWaitingThreshold = 1000;
    int           busy_waiting_count    = 0;

    while (!shutdown_flag_.load()) {
        // Regular step
        if (TryProcessQueues(thread_idx, thread_num, /* only_one */ false)) {
            busy_waiting_count = 0;
            continue;
        }

        // Stealing step
        const auto stolen_thread_idx = static_cast<std::size_t>(std::rand()) % thread_num;
        if (TryProcessQueues(stolen_thread_idx, thread_num, /* only_one */ true)) {
            busy_waiting_count = 0;
            continue;
        }

        // Check if we busy-wait
        if (++busy_waiting_count > kBusyWaitingThreshold) {
            busy_waiting_count = 0;
            WaitForMessage(std::chrono::duration_cast<std::chrono::nanoseconds>(kWaitMessageTimeout));
        }
    }
}

void CRNodeRoundRobinQueueProcessor::PrepareToRun() {
    shutdown_flag_.store(false);
    node_queues_.clear();
    Base::PrepareToRun();
    for (auto* node : GetNodes()) {
        auto queue = GetNodeQueues(node);
        node_queues_.insert(node_queues_.end(), queue.begin(), queue.end());
    }
}

void CRNodeRoundRobinQueueProcessor::CleanUp() {
    node_queues_.clear();
    Base::CleanUp();
}

std::function<void()> CRNodeRoundRobinQueueProcessor::GetWorker(std::size_t thread_idx, std::size_t thread_num) {
    return std::bind(&CRNodeRoundRobinQueueProcessor::WorkerLoop, this, thread_idx, thread_num);
}

void CRNodeRoundRobinQueueProcessor::NotifyWorkersToStop() {
    shutdown_flag_.store(true);
}

}  // namespace cris::core
