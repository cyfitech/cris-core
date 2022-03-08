#include "cris/core/logging.h"
#include "cris/core/node_runner/runner.h"

namespace cris::core {

CRNodeMainThreadRunner::CRNodeMainThreadRunner(CRNodeBase* node, std::size_t thread_num, bool auto_run)
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

std::function<void()> CRNodeMainThreadRunner::GetWorker(std::size_t thread_idx, std::size_t thread_num) {
    return [node = this->GetNode(), thread_idx, thread_num]() {
        node->MainLoop(thread_idx, thread_num);
    };
}

void CRNodeMainThreadRunner::NotifyWorkersToStop() {
    GetNode()->StopMainLoop();
}

}  // namespace cris::core
