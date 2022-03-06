#include "cris/core/node/base.h"

#include "cris/core/logging.h"

namespace cris::core {

CRMessageQueue* CRNodeBase::MessageQueueMapper(const CRMessageBasePtr& message) {
    return MessageQueueMapper(message->GetChannelId());
}

void CRNodeBase::Publish(const CRNodeBase::channel_subid_t channel_subid, CRMessageBasePtr&& message) {
    auto* message_manager = GetMessageManager();

    message->SetChannelSubId(channel_subid);

    message_manager->MessageQueueMapper(message)->AddMessage(std::move(message));
    message_manager->Kick();
}

}  // namespace cris::core
