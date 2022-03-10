#include "cris/core/node_runner/queue_processor.h"

#include "cris/core/node/base.h"

namespace cris::core {

CRNodeQueueProcessor::~CRNodeQueueProcessor() {
    CleanUp();
}

void CRNodeQueueProcessor::PrepareToRun() {
    for (auto* node : GetNodes()) {
        node->SetQueueProcessor(this);
    }
}

void CRNodeQueueProcessor::CleanUp() {
    for (auto* node : GetNodes()) {
        node->ResetQueueProcessor();
    }
}

void CRNodeQueueProcessor::Kick() {
    wait_message_cv_.notify_all();
}

void CRNodeQueueProcessor::WaitForMessage(std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lock(wait_message_mutex_);
    wait_message_cv_.wait_for(lock, timeout);
}

}  // namespace cris::core
