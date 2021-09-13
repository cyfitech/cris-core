#include "cris/core/node/multi_queue_node.h"

#include <glog/logging.h>

namespace cris::core {

CRMessageQueue *CRMultiQueueNodeBase::MessageQueueMapper(const CRMessageBasePtr &message) {
    auto queue_search = queues_.find(message->GetMessageTypeName());
    if (queue_search == queues_.end()) [[unlikely]] {
        return nullptr;
    }
    return queue_search->second.get();
}

void CRMultiQueueNodeBase::SubscribeHandler(
    std::string &&                                  message_name,
    std::function<void(const CRMessageBasePtr &)> &&callback) {
    auto insert = queues_.emplace(message_name, MakeMessageQueue(std::move(callback)));
    if (!insert.second) {
        LOG(ERROR) << __func__ << ": message '" << message_name
                   << "' is subscribed. The new callback is ignored. Node: " << this;
    }
}

std::vector<CRMessageQueue *> CRMultiQueueNodeBase::GetNodeQueues() {
    std::vector<CRMessageQueue *> queues;
    for (auto &&queue_entry : queues_) {
        queues.emplace_back(queue_entry.second.get());
    }
    return queues;
}

}  // namespace cris::core
