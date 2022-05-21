#include "cris/core/lock_queue.h"

#include "cris/core/logging.h"

#include <cstddef>
#include <mutex>
#include <utility>

namespace cris::core::impl {

LockQueueBase::LockQueueBase(std::size_t capacity) : capacity_(capacity) {
    LOG(INFO) << __func__ << ": " << this << " initialized. Capacity: " << capacity;
}

std::size_t LockQueueBase::Size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

bool LockQueueBase::IsEmpty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_ == 0;
}

bool LockQueueBase::IsFull() {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_ >= capacity_;
}

void LockQueueBase::AddImpl(std::function<void(const std::size_t)>&& add_to_index) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t                 write_pos = end_;
    ++end_;
    if (end_ >= capacity_) {
        end_ = 0;
    }
    if (size_ < capacity_) [[likely]] {
        ++size_;
    } else {
        LOG(WARNING) << __func__
                     << ": buffer is full, evicting the earliest data. "
                        "Queue: "
                     << this << ", buffer capacity: " << capacity_;
        size_  = capacity_;
        begin_ = end_;
    }

    add_to_index(write_pos);
}

void LockQueueBase::PopImpl(bool only_latest, std::function<void(const std::size_t)>&& pop_from_index) {
    std::size_t                 read_pos;
    std::lock_guard<std::mutex> lock(mutex_);

    if (size_ == 0) [[unlikely]] {
        return;
    }

    if (only_latest) {
        read_pos = end_ ? (end_ - 1) : capacity_;
        size_    = 0;
        begin_   = 0;
        end_     = 0;
    } else {
        read_pos = begin_;
        --size_;
        ++begin_;
        if (begin_ >= capacity_) {
            begin_ = 0;
        }
    }

    pop_from_index(read_pos);
}

}  // namespace cris::core::impl
