#include "cris/core/node/base.h"

#include "cris/core/logging.h"

namespace cris::core {

CRMessageQueue* CRNodeBase::MessageQueueMapper(const CRMessageBasePtr& message) {
    return MessageQueueMapper(message->GetChannelId());
}

void CRNodeBase::Publish(const CRNodeBase::channel_subid_t channel_subid, CRMessageBasePtr&& message) {
    message->SetChannelSubId(channel_subid);
    CRMessageBase::Dispatch(std::move(message));
}

}  // namespace cris::core
