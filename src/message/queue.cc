#include "message/queue.h"

namespace cris::core {

void CRMessageQueue::Process(const CRMessageBasePtr &message) {
    mProcessor(message);
}

void CRMessageQueue::PopAndProcess(bool only_latest) {
    auto message = PopMessage(only_latest);
    if (!message) {
        // TODO WARNING
        return;
    }
    Process(message);
}

}  // namespace cris::core
