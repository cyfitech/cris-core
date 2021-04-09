#include "cris/core/node/runner.h"

namespace cris::core {

std::vector<CRMessageQueue*> CRNodeRunner::GetNodeQueues() {
    return GetNode()->GetNodeQueues();
}

}  // namespace cris::core
