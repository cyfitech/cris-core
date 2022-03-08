#pragma once

#include "cris/core/message/queue.h"

#include <mutex>

namespace cris::core {

class CRMessageLockQueue : public CRMessageQueue {
   public:
    CRMessageLockQueue(std::size_t capacity, CRNodeBase* node, message_processor_t&& processor);

    void AddMessage(std::shared_ptr<CRMessageBase>&& message) override;

    CRMessageBasePtr PopMessage(bool only_latest) override;

    std::size_t Size() override;

    bool IsEmpty() override;

    bool IsFull() override;

   private:
    const std::size_t             capacity_;
    std::size_t                   size_{0};
    std::size_t                   begin_{0};
    std::size_t                   end_{0};
    std::mutex                    mutex_;
    std::vector<CRMessageBasePtr> buffer_;
};

}  // namespace cris::core
