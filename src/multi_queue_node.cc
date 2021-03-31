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

}  // namespace cris::core
