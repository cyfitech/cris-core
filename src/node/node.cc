#include "cris/core/node/node.h"

#include <glog/logging.h>

namespace cris::core {

CRNode::~CRNode() {
    for (auto&& subscribed : subscribed_) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

void CRNode::Kick() {
    wait_message_cv_.notify_all();
}

void CRNode::WaitForMessageImpl(std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lock(wait_message_mutex_);
    wait_message_cv_.wait_for(lock, timeout);
}

void CRNode::SubscribeImpl(
    const std::type_index                          message_type,
    std::function<void(const CRMessageBasePtr&)>&& callback) {
    if (!CRMessageBase::Subscribe(message_type, this)) {
        return;
    }
    subscribed_.push_back(message_type);
    return SubscribeHandler(message_type, std::move(callback));
}

}  // namespace cris::core
