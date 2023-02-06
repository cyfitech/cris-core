#include "cris/core/sched/job_lock_queue.h"

#include <bit>
#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace cris::core {

JobLockQueue::JobLockQueue(std::size_t init_capacity) : jobs_(init_capacity) {
}

JobLockQueue::~JobLockQueue() {
    JobLockQueue::ConsumeAll([](auto&&) {});
}

void JobLockQueue::ExpandUnsafe(const std::size_t target_capacity) {
    const std::size_t original_capacity = jobs_.size();
    if (target_capacity <= original_capacity) {
        // Never shrink.
        return;
    }
    const std::size_t capacity_after_resize = std::bit_ceil(target_capacity);
    jobs_.resize(capacity_after_resize);
    if (read_head_ < write_head_ || size_ == 0) {
        return;
    }
    const std::size_t read_head_after_resize = read_head_ + capacity_after_resize - original_capacity;
    std::move(
        jobs_.begin() + static_cast<long>(read_head_),
        jobs_.begin() + static_cast<long>(original_capacity),
        jobs_.begin() + static_cast<long>(read_head_after_resize));
    read_head_ = read_head_after_resize;
}

void JobLockQueue::Push(job_t job) {
    std::lock_guard lck(mtx_);
    ExpandUnsafe(size_ + 1);
    const std::size_t job_container_size    = jobs_.size();
    auto              write_head_after_push = write_head_ + 1;
    if (write_head_after_push >= job_container_size) {
        write_head_after_push -= job_container_size;
    }
    jobs_[write_head_] = std::move(job);
    write_head_        = write_head_after_push;
    ++size_;
}

void JobLockQueue::Push(std::vector<job_t> jobs) {
    std::lock_guard lck(mtx_);
    ExpandUnsafe(size_ + jobs.size());
    const std::size_t job_container_size = jobs_.size();
    for (auto& job : jobs) {
        std::size_t write_head_after_push = write_head_ + 1;
        if (write_head_after_push >= job_container_size) {
            write_head_after_push -= job_container_size;
        }
        jobs_[write_head_] = std::move(job);
        write_head_        = write_head_after_push;
    }
    size_ += jobs_.size();
}

bool JobLockQueue::ConsumeOne(const std::function<void(job_t&&)>& functor) {
    std::lock_guard lck(mtx_);
    if (size_ == 0) {
        return false;
    }
    const std::size_t job_container_size = jobs_.size();
    std::size_t read_head_after_pop = read_head_ + 1;
    if (read_head_after_pop >= job_container_size) {
        read_head_after_pop -= job_container_size;
    }
    functor(std::move(jobs_[read_head_]));
    read_head_ = read_head_after_pop;
    --size_;
    return true;
}

bool JobLockQueue::ConsumeAll(const std::function<void(job_t&&)>& functor) {
    std::lock_guard lck(mtx_);
    if (size_ == 0) {
        return false;
    }
    std::size_t idx = read_head_;
    for (std::size_t i = 0; i < size_; ++i) {
        functor(std::move(jobs_[idx]));
        ++idx;
        if (idx >= jobs_.size()) {
            idx -= jobs_.size();
        }
    }
    read_head_ = write_head_ = size_ = 0;
    return true;
}

bool JobLockQueue::Empty() {
    std::lock_guard lck(mtx_);
    return size_ == 0;
}

}  // namespace cris::core
