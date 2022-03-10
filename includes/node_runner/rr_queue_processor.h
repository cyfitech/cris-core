#pragma once

#include "cris/core/message/queue.h"
#include "cris/core/node_runner/queue_processor.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <vector>

namespace cris::core {

class CRNodeBase;

class CRNodeRoundRobinQueueProcessor : public CRNodeQueueProcessor {
   public:
    using Base = CRNodeQueueProcessor;

    CRNodeRoundRobinQueueProcessor(const std::vector<CRNodeBase*>& nodes, std::size_t thread_num);

    ~CRNodeRoundRobinQueueProcessor();

   private:
    void PrepareToRun() override;

    void CleanUp() override;

    void NotifyWorkersToStop() override;

    std::function<void()> GetWorker(std::size_t thread_idx, std::size_t thread_num) override;

    bool TryProcessQueues(const std::size_t start_index, const std::size_t stride, const bool only_one);

    void WorkerLoop(const std::size_t thread_idx, const std::size_t thread_num);

    std::vector<CRMessageQueue*> node_queues_;
    std::atomic<bool>            shutdown_flag_{false};
};

}  // namespace cris::core
