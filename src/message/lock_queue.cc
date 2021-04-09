#include "cris/core/message/lock_queue.h"

#include <glog/logging.h>

namespace cris::core {

size_t CRMessageLockQueue::Size() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mSize;
}

bool CRMessageLockQueue::IsEmpty() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mSize == 0;
}

bool CRMessageLockQueue::IsFull() {
    std::lock_guard<std::mutex> lock(mMutex);
    return mSize >= mCapacity;
}

void CRMessageLockQueue::AddMessage(std::shared_ptr<CRMessageBase> &&message) {
    std::lock_guard<std::mutex> lock(mMutex);
    size_t                      write_pos = mEnd;
    ++mEnd;
    if (mEnd >= mCapacity) {
        mEnd = 0;
    }
    if (mSize < mCapacity) {
        ++mSize;
    } else {
        LOG(WARNING) << __func__
                     << ": message buffer is full, evicting the earliest message. "
                        "Queue: "
                     << this << ", buffer capacity: " << mCapacity;
        mSize  = mCapacity;
        mBegin = mEnd;
    }

    mBuffer[write_pos] = std::move(message);
}

CRMessageBasePtr CRMessageLockQueue::PopMessage(bool only_latest) {
    CRMessageBasePtr            message;
    size_t                      read_pos;
    std::lock_guard<std::mutex> lock(mMutex);

    if (mSize == 0) {
        return nullptr;
    }

    if (only_latest) {
        read_pos = mEnd ? (mEnd - 1) : mCapacity;
        mSize    = 0;
        mBegin   = 0;
        mEnd     = 0;
    } else {
        read_pos = mBegin;
        --mSize;
        ++mBegin;
        if (mBegin >= mCapacity) {
            mBegin = 0;
        }
    }
    message = std::move(mBuffer[read_pos]);
    return message;
}

}  // namespace cris::core
