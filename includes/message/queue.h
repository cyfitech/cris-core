#pragma once

#include "cris/core/message/base.h"

#include <functional>

namespace cris::core {

class CRMessageQueue {
   public:
    using message_processor_t = std::function<void(const CRMessageBasePtr &)>;

    CRMessageQueue(CRNodeBase *node, message_processor_t &&processor);

    CRMessageQueue(const CRMessageQueue &) = delete;

    CRMessageQueue &operator=(const CRMessageQueue &) = delete;

    CRMessageQueue(CRMessageQueue &&) = delete;

    virtual ~CRMessageQueue() = default;

    virtual void AddMessage(std::shared_ptr<CRMessageBase> &&message) = 0;

    virtual CRMessageBasePtr PopMessage(bool only_latest) = 0;

    void Process(const CRMessageBasePtr &message);

    void PopAndProcess(bool only_latest);

    virtual size_t Size() = 0;

    virtual bool IsEmpty() = 0;

    virtual bool IsFull() = 0;

   protected:
    CRNodeBase *                                  node_;
    std::function<void(const CRMessageBasePtr &)> processor_;
};

template<class queue_t>
concept CRMessageQueueType = std::is_base_of_v<CRMessageQueue, queue_t>;

}  // namespace cris::core
