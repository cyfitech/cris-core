#pragma once
#include "cris/core/sched/job_queue.h"

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace cris::core {

class JobLockQueue : public JobQueueBase {
   public:
    using job_t = JobQueueBase::job_t;

    explicit JobLockQueue(std::size_t init_capacity = 8);
    ~JobLockQueue() override;

    void Push(job_t job) override;

    void Push(std::vector<job_t> jobs) override;

    bool ConsumeOne(const std::function<void(job_t&&)>& functor) override;

    bool ConsumeAll(const std::function<void(job_t&&)>& functor) override;

    bool Empty() override;

   private:
    void ExpandUnsafe(const std::size_t target_capacity);

    std::mutex         mtx_;
    std::vector<job_t> jobs_;
    std::size_t        read_head_{0};
    std::size_t        write_head_{0};
    std::size_t        size_{0};
};

}  // namespace cris::core
