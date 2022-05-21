#pragma once

#include "cris/core/lock_queue.h"
#include "cris/core/message/queue.h"

#include <mutex>

namespace cris::core {

class CRMessageLockQueue : public CRMessageQueue {
   public:
    CRMessageLockQueue(std::size_t capacity, CRNodeBase* node, message_processor_t&& processor)
        : CRMessageQueue(node, std::move(processor))
        , queue_(capacity) {}

    void AddMessage(std::shared_ptr<CRMessageBase>&& message) override { return queue_.Add(std::move(message)); }

    CRMessageBasePtr PopMessage(bool only_latest) override { return queue_.Pop(only_latest); }

    std::size_t Size() override { return queue_.Size(); }

    bool IsEmpty() override { return queue_.IsEmpty(); }

    bool IsFull() override { return queue_.IsFull(); }

   private:
    LockQueue<CRMessageBasePtr> queue_;
};

}  // namespace cris::core
