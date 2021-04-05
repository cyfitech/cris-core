#pragma once

#include <map>
#include <memory>

#include "message/lock_queue.h"
#include "node/base.h"

namespace cris::core {

class CRSingleQueueNode : public CRNodeBase {
   public:
    // TODO: make it a template if possible
    using queue_t = CRMessageLockQueue;

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
