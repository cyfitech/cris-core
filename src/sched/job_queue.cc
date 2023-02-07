#include "cris/core/sched/job_queue.h"

#include <utility>

namespace cris::core {

using job_t = JobQueue::job_t;

void JobQueue::PushBatch(std::vector<job_t> jobs) {
    for (auto& job : jobs) {
        Push(std::move(job));
    }
}

bool JobQueue::ConsumeAll(const std::function<void(job_t&&)>& functor) {
    bool consumed = false;
    while (ConsumeOne(functor)) {
        consumed = true;
    }
    return consumed;
}

}  // namespace cris::core
