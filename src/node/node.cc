#include "cris/core/node/node.h"

#include <glog/logging.h>

namespace cris::core {

CRNode::~CRNode() {
    for (auto&& subscribed : mSubscribed) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

void CRNode::Kick() {
    mWaitMessageCV.notify_all();
}

void CRNode::WaitForMessageImpl(std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lock(mWaitMessageMutex);
    mWaitMessageCV.wait_for(lock, timeout);
}

void CRNode::SubscribeImpl(std::string&& message_name, std::function<void(const CRMessageBasePtr&)>&& callback) {
    if (std::find(mSubscribed.begin(), mSubscribed.end(), message_name) != mSubscribed.end()) {
        LOG(WARNING) << __func__ << ": Message type '" << message_name << " is subscribed by the current node " << this
                     << ", skipping subscription.";
        return;
    }
    mSubscribed.push_back(message_name);

    CRMessageBase::Subscribe(message_name, this);
    return SubscribeHandler(std::move(message_name), std::move(callback));
}

}  // namespace cris::core
