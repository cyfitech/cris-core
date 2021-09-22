#include "cris/core/node_runner/runner.h"

#include <glog/logging.h>

namespace cris::core {

std::vector<CRMessageQueue*> CRNodeRunnerBase::GetNodeQueues() {
    return GetNode()->GetNodeQueues();
}

CRMultiThreadNodeRunner::CRMultiThreadNodeRunner(CRNodeBase* node, size_t thread_num)
    : CRNodeRunnerBase()
    , mNode(node)
    , mThreadNum(thread_num) {
    LOG(INFO) << __func__ << ": " << this << " initialized for " << mNode->GetName() << "(" << mNode << "), "
              << "thread number: " << mThreadNum;
}

CRMultiThreadNodeRunner::~CRMultiThreadNodeRunner() {
    Join();
}

CRNodeBase* CRMultiThreadNodeRunner::GetNode() const {
    return mNode;
}

size_t CRMultiThreadNodeRunner::GetThreadNum() const {
    return mThreadNum;
}

void CRMultiThreadNodeRunner::Run() {
    std::lock_guard<std::mutex>  state_lock(mRunStateMutex);
    std::unique_lock<std::mutex> threads_lock(mRunThreadsMutex, std::try_to_lock);
    if (!threads_lock.owns_lock()) {
        LOG(ERROR) << __func__ << ": Failed to run, maybe others are running/joining. Runner: " << this;
        return;
    }

    if (mIsRunning) {
        LOG(WARNING) << __func__ << ": Runner " << this << " is currently running.";
        return;
    }
    mIsRunning = true;

    LOG(INFO) << __func__ << ": runner " << this << " start running.";

    PrepareToRun();
    for (size_t i = 0; i < mThreadNum; ++i) {
        mWorkerThreads.emplace_back(GetWorker(i, mThreadNum));
    }
}

void CRMultiThreadNodeRunner::Stop() {
    std::lock_guard<std::mutex> lock(mRunStateMutex);
    if (!mIsRunning) {
        LOG(WARNING) << __func__ << ": Runner " << this << " is currently not running.";
        return;
    }
    mIsRunning = false;
    NotifyWorkersToStop();

    LOG(INFO) << __func__ << ": runner " << this << " about to stop.";
}

void CRMultiThreadNodeRunner::Join() {
    std::lock_guard<std::mutex> lock(mRunThreadsMutex);
    for (auto&& thread : mWorkerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    mWorkerThreads.clear();
}

}  // namespace cris::core
