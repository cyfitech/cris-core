#include "cris/core/message/queue.h"

#include <glog/logging.h>

namespace cris::core {

void CRMessageQueue::Process(const CRMessageBasePtr &message) {
    mProcessor(message);
}

void CRMessageQueue::PopAndProcess(bool only_latest) {
    auto message = PopMessage(only_latest);
    if (!message) {
        DLOG(INFO) << __func__ << ": no message in queue " << this;
        return;
    }
    Process(message);
}

}  // namespace cris::core
