#pragma once

#include <map>
#include <memory>

#include "cris/core/message/lock_queue.h"
#include "cris/core/node/node.h"

namespace cris::core {

template<CRMessageQueueType queue_t = CRMessageLockQueue>
class CRSingleQueueNode : public CRNode {
   public:
    CRSingleQueueNode(size_t                                          queue_capacity,
                      std::function<void(const CRMessageBasePtr &)> &&callback)
        : mQueueCapacity(queue_capacity)
        , mQueue(mQueueCapacity, this, std::move(callback)) {}

    CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) override { return &mQueue; }

   protected:
    std::vector<CRMessageQueue *> GetNodeQueues() override {
        std::vector<CRMessageQueue *> queues = {&mQueue};
        return queues;
    }

    size_t  mQueueCapacity;
    queue_t mQueue;
};

}  // namespace cris::core
