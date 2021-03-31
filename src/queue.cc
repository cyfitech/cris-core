#include "message/queue.h"

namespace cris::core {

void CRMessageQueue::Process(const CRMessageBasePtr &message) {
    mProcessor(message);
}

void CRMessageQueue::PopAndProcess(bool only_latest) {
    Process(PopMessage(only_latest));
}

}  // namespace cris::core
