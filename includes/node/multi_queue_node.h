#pragma once

#include <map>
#include <memory>

#include "cris/core/message/lock_queue.h"
#include "cris/core/node/node.h"

namespace cris::core {

class CRMultiQueueNodeBase : public CRNode {
   public:
    explicit CRMultiQueueNodeBase(size_t queue_capacity) : mQueueCapacity(queue_capacity) {}

    CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) override;

   protected:
    using queue_callback_t = std::function<void(const CRMessageBasePtr &)>;

    virtual std::unique_ptr<CRMessageQueue> MakeMessageQueue(queue_callback_t &&callback) = 0;

    void SubscribeHandler(std::string &&                                  message_name,
                          std::function<void(const CRMessageBasePtr &)> &&callback) override;

    std::vector<CRMessageQueue *> GetNodeQueues() override;

    size_t                                          mQueueCapacity;
    std::map<std::string, std::unique_ptr<CRMessageQueue>> mQueues{};
};

template<CRMessageQueueType queue_t = CRMessageLockQueue>
class CRMultiQueueNode : public CRMultiQueueNodeBase {
   public:
    explicit CRMultiQueueNode(size_t queue_capacity) : CRMultiQueueNodeBase(queue_capacity) {}

   private:
    using queue_callback_t = CRMultiQueueNodeBase::queue_callback_t;

    std::unique_ptr<CRMessageQueue> MakeMessageQueue(queue_callback_t &&callback) override;
};

template<CRMessageQueueType queue_t>
std::unique_ptr<CRMessageQueue> CRMultiQueueNode<queue_t>::MakeMessageQueue(
    queue_callback_t &&callback) {
    return std::make_unique<queue_t>(mQueueCapacity, this, std::move(callback));
}

}  // namespace cris::core
