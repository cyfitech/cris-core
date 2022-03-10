#include "cris/core/node/node.h"

#include "cris/core/logging.h"

namespace cris::core {

CRNode::~CRNode() {
    for (auto&& subscribed : subscribed_) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

void CRNode::SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) {
    if (!CRMessageBase::Subscribe(channel, this)) {
        return;
    }
    subscribed_.push_back(channel);
    return SubscribeHandler(channel, std::move(callback));
}

void CRNode::Kick() {
    auto* queue_processor = GetQueueProcessor();
    if (queue_processor) {
        queue_processor->Kick();
    }
}

void CRNode::SetQueueProcessor(CRNodeQueueProcessor* queue_processor) {
    if (queue_processor && queue_processor_) {
        LOG(ERROR) << __func__ << ": current runner " << queue_processor_ << " is busy.";
    }
    queue_processor_ = queue_processor;
}

CRNodeQueueProcessor* CRNode::GetQueueProcessor() {
    return queue_processor_;
}

void CRNode::SetMainLoopRunner(CRNodeMainLoopRunner* main_loop_runner) {
    if (main_loop_runner && main_loop_runner_) {
        LOG(ERROR) << __func__ << ": current runner " << main_loop_runner_ << " is busy.";
    }
    main_loop_runner_ = main_loop_runner;
}

CRNodeMainLoopRunner* CRNode::GetMainLoopRunner() {
    return main_loop_runner_;
}

}  // namespace cris::core
