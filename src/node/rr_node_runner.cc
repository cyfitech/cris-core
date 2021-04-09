#include <glog/logging.h>

#include "node/runner.h"

namespace cris::core {

using std::literals::chrono_literals::operator""ms;

static constexpr auto kWaitMessageTimeout = 500ms;

CRNodeRoundRobinRunner::CRNodeRoundRobinRunner(CRNodeBase* node, size_t thread_num, bool auto_run)
    : CRNodeRunner()
    , mNode(node)
    , mThreadNum(thread_num) {
    if (auto_run) {
        Run();
    }
}

CRNodeRoundRobinRunner::~CRNodeRoundRobinRunner() {
    Stop();
    Join();
}

CRNodeBase* CRNodeRoundRobinRunner::GetNode() {
    return mNode;
}

void CRNodeRoundRobinRunner::Run() {
    std::lock_guard<std::mutex>  state_lock(mRunStateMutex);
    std::unique_lock<std::mutex> threads_lock(mRunThreadsMutex, std::try_to_lock);
    if (!threads_lock.owns_lock()) {
        LOG(ERROR) << __func__
                   << ": Failed to run, maybe others are running/joining. Runner: " << this;
        return;
    }

    if (mIsRunning) {
        LOG(WARNING) << __func__ << ": Runner " << this << " is currently running.";
        return;
    }
    mIsRunning = true;
    mShutdownFlag.store(false);
    mNodeQueues = GetNodeQueues();
    for (size_t i = 0; i < mThreadNum; ++i) {
        mWorkerThreads.emplace_back(std::bind(&CRNodeRoundRobinRunner::WorkerLoop, this, i));
    }
}

void CRNodeRoundRobinRunner::Stop() {
    std::lock_guard<std::mutex> lock(mRunStateMutex);
    if (!mIsRunning) {
        LOG(WARNING) << __func__ << ": Runner " << this << " is currently not running.";
        return;
    }
    mIsRunning = false;
    mShutdownFlag.store(true);
}

void CRNodeRoundRobinRunner::Join() {
    std::lock_guard<std::mutex> lock(mRunThreadsMutex);
    for (auto&& thread : mWorkerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    mWorkerThreads.clear();
}

void CRNodeRoundRobinRunner::WorkerLoop(size_t thread_idx) {
    while (!mShutdownFlag.load()) {
        bool hasMessage = false;
        for (size_t i = thread_idx; i < mNodeQueues.size(); i += mThreadNum) {
            auto queue = mNodeQueues[i];
            if (queue->IsEmpty()) {
                continue;
            }
            hasMessage = true;
            queue->PopAndProcess(false);
        }
        if (!hasMessage) {
            GetNode()->WaitForMessage(kWaitMessageTimeout);
        }
    }
}

}  // namespace cris::core
