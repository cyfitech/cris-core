#include "cris/core/node_runner/main_loop_runner.h"

#include "cris/core/logging.h"
#include "cris/core/node/base.h"

namespace cris::core {

CRNodeMainLoopRunner::CRNodeMainLoopRunner(CRNodeBase* node, std::size_t thread_num)
    : CRMultiThreadNodeRunner({node}, thread_num) {
    LOG(INFO) << __func__ << ": " << this << " initialized";
}

CRNodeMainLoopRunner::~CRNodeMainLoopRunner() {
    Stop();
    Join();
}

void CRNodeMainLoopRunner::PrepareToRun() {
    for (auto* node : GetNodes()) {
        node->SetMainLoopRunner(this);
    }
}

void CRNodeMainLoopRunner::CleanUp() {
    for (auto* node : GetNodes()) {
        node->ResetMainLoopRunner();
    }
}

std::function<void()> CRNodeMainLoopRunner::GetWorker(std::size_t thread_idx, std::size_t thread_num) {
    return [node = this->GetNode(), thread_idx, thread_num]() {
        if (node) {
            node->MainLoop(thread_idx, thread_num);
        }
    };
}

void CRNodeMainLoopRunner::NotifyWorkersToStop() {
    for (auto* node : GetNodes()) {
        node->StopMainLoop();
    }
}

CRNodeBase* CRNodeMainLoopRunner::GetNode() {
    const auto& nodes = GetNodes();
    if (nodes.empty()) {
        LOG(ERROR) << __func__ << ": runner " << this << " has no binding node.";
        return nullptr;
    }
    if (nodes.size() > 1) {
        LOG(WARNING) << __func__ << ": runner " << this << " has more than 1 binding nodes."
                     << " Actual: " << nodes.size() << ". The extra nodes will be ignored.";
    }
    return nodes[0];
}

}  // namespace cris::core
