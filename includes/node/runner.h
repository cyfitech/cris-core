#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "node/base.h"

namespace cris::core {

class CRNodeRunner {
   public:
    CRNodeRunner() = default;

    CRNodeRunner(const CRNodeRunner&) = delete;

    CRNodeRunner(CRNodeRunner&&) = default;

    CRNodeRunner& operator=(const CRNodeRunner&) = delete;

    virtual ~CRNodeRunner() = default;

    virtual CRNodeBase* GetNode() = 0;

    std::vector<CRMessageQueue*> GetNodeQueues();

    virtual void Run() = 0;

    virtual void Stop() = 0;

    virtual void Join() = 0;
};

class CRNodeRoundRobinRunner : public CRNodeRunner {
   public:
    CRNodeRoundRobinRunner(CRNodeBase* node, size_t thread_num, bool auto_run = false);

    ~CRNodeRoundRobinRunner();

    CRNodeBase* GetNode() override;

    void Run() override;

    void Stop() override;

    void Join() override;

   private:
    void WorkerLoop(size_t thread_idx);

    CRNodeBase*                  mNode;
    std::vector<CRMessageQueue*> mNodeQueues;

    size_t                   mThreadNum;
    std::vector<std::thread> mWorkerThreads;

    bool              mIsRunning{false};
    std::atomic<bool> mShutdownFlag{false};
    std::mutex        mRunStateMutex;
    std::mutex        mRunThreadsMutex;
};

}  // namespace cris::core
