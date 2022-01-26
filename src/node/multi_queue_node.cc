#include "cris/core/node/multi_queue_node.h"

#include <glog/logging.h>

namespace cris::core {

CRMessageQueue* CRMultiQueueNodeBase::MessageQueueMapper(const channel_id_t channel) {
    auto queue_search = queues_.find(channel);
    if (queue_search == queues_.end()) [[unlikely]] {
        return nullptr;
    }
    return queue_search->second.get();
}

void CRMultiQueueNodeBase::SubscribeHandler(
    const channel_id_t                             channel,
    std::function<void(const CRMessageBasePtr&)>&& callback) {
    auto insert = queues_.emplace(channel, MakeMessageQueue(std::move(callback)));
    if (!insert.second) {
        LOG(ERROR) << __func__ << ": channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is subscribed. The new callback is ignored. Node: " << this;
    }
}

std::vector<CRMessageQueue*> CRMultiQueueNodeBase::GetNodeQueues() {
    std::vector<CRMessageQueue*> queues;
    for (auto&& queue_entry : queues_) {
        queues.emplace_back(queue_entry.second.get());
    }
    return queues;
}

}  // namespace cris::core
