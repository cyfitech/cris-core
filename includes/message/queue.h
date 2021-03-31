#pragma once

#include <functional>

#include "message/base.h"

namespace cris::core {

class CRMessageQueue {
   public:
    using message_processor_t = std::function<void(const CRMessageBasePtr &)>;

    CRMessageQueue(CRNodeBase *node, message_processor_t &&processor)
        : mNode(node)
        , mProcessor(std::move(processor)) {}

    CRMessageQueue(const CRMessageQueue &) = delete;

    CRMessageQueue &operator=(const CRMessageQueue &) = delete;

    CRMessageQueue(CRMessageQueue &&) = delete;

    virtual ~CRMessageQueue() = default;

    virtual void AddMessage(std::shared_ptr<CRMessageBase> &&message) = 0;

    virtual CRMessageBasePtr PopMessage(bool only_latest) = 0;

    void Process(const CRMessageBasePtr &message);

    void PopAndProcess(bool only_latest);

   private:
    CRNodeBase *                                  mNode;
    std::function<void(const CRMessageBasePtr &)> mProcessor;
};

}  // namespace cris::core
