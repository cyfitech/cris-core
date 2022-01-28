#pragma once

#include "cris/core/message/lock_queue.h"
#include "cris/core/node/node.h"

#include <boost/functional/hash.hpp>

#include <cstddef>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace cris::core {

class CRMultiQueueNodeBase : public CRNode {
   public:
    explicit CRMultiQueueNodeBase(size_t queue_capacity) : queue_capacity_(queue_capacity) {}

    CRMessageQueue* MessageQueueMapper(const channel_id_t channel) override;

   protected:
    using queue_callback_t = std::function<void(const CRMessageBasePtr&)>;
    using queue_map_t = std::unordered_map<channel_id_t, std::unique_ptr<CRMessageQueue>, boost::hash<channel_id_t>>;

    virtual std::unique_ptr<CRMessageQueue> MakeMessageQueue(queue_callback_t&& callback) = 0;

    void SubscribeHandler(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) override;

    std::vector<CRMessageQueue*> GetNodeQueues() override;

    std::size_t queue_capacity_;
    queue_map_t queues_;
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
