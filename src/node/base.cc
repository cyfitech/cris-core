#include "cris/core/node/base.h"

#include <glog/logging.h>

namespace cris::core {

void CRNodeBase::Publish(CRMessageBasePtr&& message) {
    auto* message_manager = GetMessageManager();
    message_manager->MessageQueueMapper(message)->AddMessage(std::move(message));
    message_manager->Kick();
}

}  // namespace cris::core
