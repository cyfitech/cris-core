#include "cris/core/node/base.h"

#include <glog/logging.h>

namespace cris::core {

CRNodeBase::~CRNodeBase() {
    for (auto&& subscribed : mSubscribed) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

void CRNodeBase::Kick() {
    mWaitMessageCV.notify_all();
}

void CRNodeBase::Publish(CRMessageBasePtr&& message) {
    auto* message_manager = GetMessageManager();
    message_manager->MessageQueueMapper(message)->AddMessage(std::move(message));
    message_manager->Kick();
}

void CRNodeBase::SubscribeImpl(std::string&&                                  message_name,
                               std::function<void(const CRMessageBasePtr&)>&& callback) {
    if (std::find(mSubscribed.begin(), mSubscribed.end(), message_name) != mSubscribed.end()) {
        LOG(WARNING) << __func__ << ": Message type '" << message_name
                     << " is subscribed by the current node " << this << ", skipping subscription.";
        return;
    }
    mSubscribed.push_back(message_name);

    CRMessageBase::Subscribe(message_name, this);
    return SubscribeHandler(std::move(message_name), std::move(callback));
}

}  // namespace cris::core
