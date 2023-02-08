#pragma once

#include "cris/core/sched/job_runner.h"

#include <functional>
#include <vector>

namespace cris::core {

class JobQueue {
   public:
    using job_t = JobRunner::job_t;

    virtual ~JobQueue() = default;

    virtual void Push(job_t job) = 0;

    virtual void PushBatch(std::vector<job_t> jobs);

    virtual bool ConsumeOne(const std::function<void(job_t&&)>& functor) = 0;

    virtual bool ConsumeAll(const std::function<void(job_t&&)>& functor);

    virtual bool Empty() = 0;
};

}  // namespace cris::core
