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
    const size_t                  capacity_;
    size_t                        size_{0};
    size_t                        begin_{0};
    size_t                        end_{0};
    std::mutex                    mutex_;
    std::vector<CRMessageBasePtr> buffer_;
};

}  // namespace cris::core
