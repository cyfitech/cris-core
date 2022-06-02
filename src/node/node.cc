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

std::optional<CRNode::msg_callback_t> CRNode::GetCallback(const CRMessageBasePtr& message) {
    if (!message) {
        return std::nullopt;
    }

    const auto channel = message->GetChannelId();

    auto callback_search_result = callbacks_.find(channel);
    if (callback_search_result == callbacks_.end()) {
        LOG(ERROR) << __func__ << ": message channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is not subscribed by node " << GetName() << "(" << this << ").";
        return std::nullopt;
    }
    return callback_search_result->second;
}

std::optional<CRNode::job_t> CRNode::GenerateJob(const CRMessageBasePtr& message) {
    if (auto callback_opt = GetCallback(message)) {
        return [callback = std::move(*callback_opt), message]() {
            callback(message);
        };
    }
    return std::nullopt;
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
    auto callback_insert = callbacks_.emplace(channel, std::move(callback));
    if (!callback_insert.second) {
        LOG(ERROR) << __func__ << ": channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is subscribed. The new callback is ignored. Node: " << GetName() << "(" << this << ").";
        return;
    }

    // TODO(hao.chen): Yes, I know it is weird, but we are deprecating the queues. They will be removed soon.
    CHECK(queues_.emplace(channel, MakeMessageQueue(callbacks_[channel])).second);
}

CRMessageQueue* CRNode::MessageQueueMapper(const channel_id_t channel) {
    auto queue_search = queues_.find(channel);
    if (queue_search == queues_.end()) [[unlikely]] {
        return nullptr;
    }
    return queue_search->second.get();
}

std::vector<CRMessageQueue*> CRNode::GetNodeQueues() {
    std::vector<CRMessageQueue*> queues;
    for (auto&& queue_entry : queues_) {
        queues.emplace_back(queue_entry.second.get());
    }
    return queues;
}

}  // namespace cris::core
