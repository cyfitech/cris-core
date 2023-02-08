#include "cris/core/sched/job_lockfree_queue.h"

#include <functional>
#include <utility>

namespace cris::core {

JobLockFreeQueue::JobLockFreeQueue(std::size_t init_capacity) : jobs_(init_capacity) {
}

JobLockFreeQueue::~JobLockFreeQueue() {
    JobLockFreeQueue::ConsumeAll([](auto&&) {});
}

void JobLockFreeQueue::Push(job_t job) {
    jobs_.push(new job_t(std::move(job)));
}

bool JobLockFreeQueue::ConsumeOne(const std::function<void(job_t&&)>& functor) {
    return jobs_.consume_one([&functor](job_t* const job_ptr) {
        functor(std::move(*job_ptr));
        delete job_ptr;
    });
}

bool JobLockFreeQueue::ConsumeAll(const std::function<void(job_t&&)>& functor) {
    return jobs_.consume_all([&functor](job_t* const job_ptr) {
        functor(std::move(*job_ptr));
        delete job_ptr;
    });
}

bool JobLockFreeQueue::Empty() {
    return jobs_.empty();
}

}  // namespace cris::core
