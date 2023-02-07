#pragma once
#include "cris/core/sched/job_queue.h"

#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wambiguous-reversed-operator"
#endif

#include <boost/lockfree/queue.hpp>

#if defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace cris::core {

class JobLockFreeQueue : public JobQueue {
   public:
    using job_t = JobQueue::job_t;

    explicit JobLockFreeQueue(std::size_t init_capacity = 8);
    ~JobLockFreeQueue() override;

    void Push(job_t job) override;

    bool ConsumeOne(const std::function<void(job_t&&)>& functor) override;

    bool ConsumeAll(const std::function<void(job_t&&)>& functor) override;

    bool Empty() override;

   private:
    boost::lockfree::queue<job_t*> jobs_;
};

}  // namespace cris::core
