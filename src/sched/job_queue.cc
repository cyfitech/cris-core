#include "cris/core/sched/job_queue.h"

#include <utility>

namespace cris::core {

using job_t = JobQueueBase::job_t;

void JobQueueBase::PushBatch(std::vector<job_t> jobs) {
    for (auto& job : jobs) {
        Push(std::move(job));
    }
}

bool JobQueueBase::ConsumeAll(const std::function<void(job_t&&)>& functor) {
    bool consumed = false;
    while (ConsumeOne(functor)) {
        consumed = true;
    }
    return consumed;
}

}  // namespace cris::core
