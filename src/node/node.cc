#include "cris/core/node.h"

#include "cris/core/logging.h"

#include <mutex>

namespace cris::core {

CRNode::~CRNode() {
    for (auto&& subscribed : subscribed_) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

CRMessageQueue* CRNode::MessageQueueMapper(const CRMessageBasePtr& message) {
    return MessageQueueMapper(message->GetChannelId());
}

void CRNode::Publish(const CRNode::channel_subid_t channel_subid, CRMessageBasePtr&& message) {
    message->SetChannelSubId(channel_subid);
    CRMessageBase::Dispatch(std::move(message));
}

void CRNode::Kick() {
    wait_message_cv_.notify_all();
}

void CRNode::WaitForMessageImpl(std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lock(wait_message_mutex_);
    wait_message_cv_.wait_for(lock, timeout);
}

void CRNode::SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) {
    if (!CRMessageBase::Subscribe(channel, this)) {
        return;
    }
    subscribed_.push_back(channel);
    return SubscribeHandler(channel, std::move(callback));
}

CRMessageQueue* CRNode::MessageQueueMapper(const channel_id_t channel) {
    auto queue_search = queues_.find(channel);
    if (queue_search == queues_.end()) [[unlikely]] {
        return nullptr;
    }
    return queue_search->second.get();
}

void CRNode::SubscribeHandler(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) {
    auto insert = queues_.emplace(channel, MakeMessageQueue(std::move(callback)));
    if (!insert.second) {
        LOG(ERROR) << __func__ << ": channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is subscribed. The new callback is ignored. Node: " << this;
    }
}

std::vector<CRMessageQueue*> CRNode::GetNodeQueues() {
    std::vector<CRMessageQueue*> queues;
    for (auto&& queue_entry : queues_) {
        queues.emplace_back(queue_entry.second.get());
    }
    return queues;
}

}  // namespace cris::core
