#pragma once

#include "cris/core/message/lock_queue.h"
#include "cris/core/node/node.h"

#include <memory>
#include <typeindex>
#include <unordered_map>

namespace cris::core {

class CRMultiQueueNodeBase : public CRNode {
   public:
    explicit CRMultiQueueNodeBase(size_t queue_capacity) : queue_capacity_(queue_capacity) {}

    CRMessageQueue* MessageQueueMapper(const CRMessageBasePtr& message) override;

   protected:
    using queue_callback_t = std::function<void(const CRMessageBasePtr&)>;

    virtual std::unique_ptr<CRMessageQueue> MakeMessageQueue(queue_callback_t&& callback) = 0;

    void SubscribeHandler(const std::type_index message_type, std::function<void(const CRMessageBasePtr&)>&& callback)
        override;

    std::vector<CRMessageQueue*> GetNodeQueues() override;

    size_t                                                               queue_capacity_;
    std::unordered_map<std::type_index, std::unique_ptr<CRMessageQueue>> queues_{};
};

template<CRMessageQueueType queue_t = CRMessageLockQueue>
class CRMultiQueueNode : public CRMultiQueueNodeBase {
   public:
    explicit CRMultiQueueNode(size_t queue_capacity) : CRMultiQueueNodeBase(queue_capacity) {}

   private:
    using queue_callback_t = CRMultiQueueNodeBase::queue_callback_t;

    std::unique_ptr<CRMessageQueue> MakeMessageQueue(queue_callback_t&& callback) override;
};

template<CRMessageQueueType queue_t>
std::unique_ptr<CRMessageQueue> CRMultiQueueNode<queue_t>::MakeMessageQueue(queue_callback_t&& callback) {
    return std::make_unique<queue_t>(queue_capacity_, this, std::move(callback));
}

}  // namespace cris::core
