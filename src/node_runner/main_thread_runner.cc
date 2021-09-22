#include "cris/core/node_runner/runner.h"

#include <glog/logging.h>

namespace cris::core {

CRNodeMainThreadRunner::CRNodeMainThreadRunner(CRNodeBase* node, size_t thread_num, bool auto_run)
    : CRMultiThreadNodeRunner(node, thread_num) {
    LOG(INFO) << __func__ << ": " << this << " initialized";
    if (auto_run) {
        Run();
    }
}

CRNodeMainThreadRunner::~CRNodeMainThreadRunner() {
    Stop();
    Join();
}

void CRNodeMainThreadRunner::PrepareToRun() {
}

std::function<void()> CRNodeMainThreadRunner::GetWorker(size_t thread_idx, size_t thread_num) {
    return [node = this->GetNode(), thread_idx, thread_num]() {
        node->MainLoop(thread_idx, thread_num);
    };
}

void CRNodeMainThreadRunner::NotifyWorkersToStop() {
    GetNode()->StopMainLoop();
}

}  // namespace cris::core
