#pragma once

#include "cris/core/message/lock_queue.h"
#include "cris/core/node/node.h"

#include <memory>

namespace cris::core {

template<CRMessageQueueType queue_t = CRMessageLockQueue>
class CRSingleQueueNode : public CRNode {
   public:
    CRSingleQueueNode(size_t queue_capacity, std::function<void(const CRMessageBasePtr&)>&& callback)
        : queue_capacity_(queue_capacity)
        , queue_(queue_capacity_, this, std::move(callback)) {}

    CRMessageQueue* MessageQueueMapper(const CRMessageBasePtr& message) override { return &queue_; }

   protected:
    std::vector<CRMessageQueue*> GetNodeQueues() override {
        std::vector<CRMessageQueue*> queues = {&queue_};
        return queues;
    }

    size_t  queue_capacity_;
    queue_t queue_;
};

}  // namespace cris::core
