#include "node/multi_queue_node.h"

namespace cris::core {

CRMessageQueue *CRMultiQueueNode::MessageQueueMapper(const CRMessageBasePtr &message) {
    auto queue_search = mQueues.find(message->GetMessageTypeName());
    if (queue_search == mQueues.end()) {
        return nullptr;
    }
    return queue_search->second.get();
}

void CRMultiQueueNode::SubscribeImpl(std::string &&                                  message_name,
                                     std::function<void(const CRMessageBasePtr &)> &&callback) {
    auto insert = mQueues.emplace(
        message_name, std::make_unique<queue_t>(mQueueCapacity, this, std::move(callback)));
    if (!insert.second) {
        // Insertion failure, topic subscribed
        // TODO WARNING
    }
}

std::vector<CRMessageQueue *> CRMultiQueueNode::GetNodeQueues() {
    std::vector<CRMessageQueue *> queues;
    for (auto &&queue_entry : mQueues) {
        queues.emplace_back(queue_entry.second.get());
    }
    return queues;
}

}  // namespace cris::core
