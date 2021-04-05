#pragma once

#include <map>
#include <memory>

#include "message/lock_queue.h"
#include "node/base.h"

namespace cris::core {

class CRMultiQueueNode : public CRNodeBase {
   public:
    // TODO: make it a template if possible
    using queue_t = CRMessageLockQueue;

    explicit CRMultiQueueNode(size_t queue_capacity) : mQueueCapacity(queue_capacity) {}

    CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) override;

   private:
    void SubscribeImpl(std::string &&                                  message_name,
                       std::function<void(const CRMessageBasePtr &)> &&callback) override;

    std::vector<CRMessageQueue *> GetNodeQueues() override;

    size_t                                          mQueueCapacity;
    std::map<std::string, std::unique_ptr<queue_t>> mQueues{};
};

}  // namespace cris::core
