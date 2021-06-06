#pragma once

#include <mutex>

#include "cris/core/message/queue.h"

namespace cris::core {

class CRMessageLockQueue : public CRMessageQueue {
   public:
    CRMessageLockQueue(size_t capacity, CRNodeBase *node, message_processor_t &&processor);

    void AddMessage(std::shared_ptr<CRMessageBase> &&message) override;

    CRMessageBasePtr PopMessage(bool only_latest) override;

    size_t Size() override;

    bool IsEmpty() override;

    bool IsFull() override;

   private:
    const size_t                  mCapacity;
    size_t                        mSize{0};
    size_t                        mBegin{0};
    size_t                        mEnd{0};
    std::mutex                    mMutex;
    std::vector<CRMessageBasePtr> mBuffer;
};

}  // namespace cris::core
