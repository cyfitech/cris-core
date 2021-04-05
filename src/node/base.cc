#include "node/base.h"

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

}  // namespace cris::core
