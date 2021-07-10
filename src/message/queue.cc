#include "cris/core/message/queue.h"

#include <glog/logging.h>

#include "cris/core/node/base.h"

namespace cris::core {

CRMessageQueue::CRMessageQueue(CRNodeBase *node, message_processor_t &&processor)
    : mNode(node)
    , mProcessor(std::move(processor)) {
    auto node_name = mNode ? mNode->GetName() : "null";
    LOG(INFO) << __func__ << ": " << this << " initialized for node " << node_name << "(" << mNode
              << ")";
}

void CRMessageQueue::Process(const CRMessageBasePtr &message) {
    mProcessor(message);
}

void CRMessageQueue::PopAndProcess(bool only_latest) {
    auto message = PopMessage(only_latest);
    if (!message) [[unlikely]] {
        DLOG(INFO) << __func__ << ": no message in queue " << this;
        return;
    }
    Process(message);
}

}  // namespace cris::core
