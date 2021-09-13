#include "cris/core/message/lock_queue.h"

#include <glog/logging.h>

namespace cris::core {

CRMessageLockQueue::CRMessageLockQueue(size_t capacity, CRNodeBase *node, message_processor_t &&processor)
    : CRMessageQueue(node, std::move(processor))
    , capacity_(capacity) {
    buffer_.resize(capacity_, nullptr);
    LOG(INFO) << __func__ << ": " << this << " initialized. Capacity: " << capacity;
}

size_t CRMessageLockQueue::Size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_;
}

bool CRMessageLockQueue::IsEmpty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_ == 0;
}

bool CRMessageLockQueue::IsFull() {
    std::lock_guard<std::mutex> lock(mutex_);
    return size_ >= capacity_;
}

void CRMessageLockQueue::AddMessage(std::shared_ptr<CRMessageBase> &&message) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t                      write_pos = end_;
    ++end_;
    if (end_ >= capacity_) {
        end_ = 0;
    }
    if (size_ < capacity_) [[likely]] {
        ++size_;
    } else {
        LOG(WARNING) << __func__
                     << ": message buffer is full, evicting the earliest message. "
                        "Queue: "
                     << this << ", buffer capacity: " << capacity_;
        size_  = capacity_;
        begin_ = end_;
    }

    buffer_[write_pos] = std::move(message);
}

CRMessageBasePtr CRMessageLockQueue::PopMessage(bool only_latest) {
    CRMessageBasePtr            message;
    size_t                      read_pos;
    std::lock_guard<std::mutex> lock(mutex_);

    if (size_ == 0) [[unlikely]] {
        return nullptr;
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
    message = std::move(buffer_[read_pos]);
    return message;
}

}  // namespace cris::core
