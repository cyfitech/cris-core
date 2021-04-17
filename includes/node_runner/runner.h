#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "cris/core/node/base.h"

namespace cris::core {

class CRNodeRunnerBase {
   public:
    CRNodeRunnerBase() = default;

    CRNodeRunnerBase(const CRNodeRunnerBase&) = delete;

    CRNodeRunnerBase(CRNodeRunnerBase&&) = default;

    CRNodeRunnerBase& operator=(const CRNodeRunnerBase&) = delete;

    virtual ~CRNodeRunnerBase() = default;

    virtual CRNodeBase* GetNode() const = 0;

    std::vector<CRMessageQueue*> GetNodeQueues();

    virtual void Run() = 0;

    virtual void Stop() = 0;

    virtual void Join() = 0;
};

class CRMultiThreadNodeRunner : public CRNodeRunnerBase {
   public:
    CRMultiThreadNodeRunner(CRNodeBase* node, size_t thread_num);

    ~CRMultiThreadNodeRunner();

    CRNodeBase* GetNode() const override;

    size_t GetThreadNum() const;

    void Run() override;

    void Stop() override;

    void Join() override;

    virtual void PrepareToRun() = 0;

    virtual std::function<void()> GetWorker(size_t thread_idx, size_t thread_num) = 0;

    virtual void NotifyWorkersToStop() = 0;

   private:
    CRNodeBase*              mNode;
    size_t                   mThreadNum;
    std::vector<std::thread> mWorkerThreads;
    bool                     mIsRunning{false};
    std::mutex               mRunStateMutex;
    std::mutex               mRunThreadsMutex;
};

class CRNodeMainThreadRunner : public CRMultiThreadNodeRunner {
   public:
    CRNodeMainThreadRunner(CRNodeBase* node, size_t thread_num, bool auto_run = false);

    ~CRNodeMainThreadRunner();

    void PrepareToRun() override;

    std::function<void()> GetWorker(size_t thread_idx, size_t thread_num) override;

    void NotifyWorkersToStop() override;
};

class CRNodeRoundRobinQueueProcessor : public CRMultiThreadNodeRunner {
   public:
    CRNodeRoundRobinQueueProcessor(CRNodeBase* node, size_t thread_num, bool auto_run = false);

    ~CRNodeRoundRobinQueueProcessor();

    void PrepareToRun() override;

    std::function<void()> GetWorker(size_t thread_idx, size_t thread_num) override;

    void NotifyWorkersToStop() override;

   private:
    void WorkerLoop(const size_t thread_idx, const size_t thread_num);

    std::vector<CRMessageQueue*> mNodeQueues;
    std::atomic<bool>            mShutdownFlag{false};
};

template<class runner_t>
concept CRNodeRunnerType = std::is_base_of_v<CRNodeRunnerBase, runner_t>;

template<CRNodeRunnerType queue_processor_t,
         CRNodeRunnerType main_thread_runner_t = CRNodeMainThreadRunner>
class CRNodeRunner : public CRNodeRunnerBase {
   public:
    CRNodeRunner(CRNodeBase* node,
                 size_t      queue_processor_thread_num,
                 size_t      main_thread_num = 1,
                 bool        auto_run        = false)
        : mNode(node)
        , mQueueProcessor(node, queue_processor_thread_num, auto_run)
        , mMainThreadRunner(node, main_thread_num, auto_run) {}

    void Run() override {
        mQueueProcessor.Run();
        mMainThreadRunner.Run();
    }

    void Stop() override {
        mQueueProcessor.Stop();
        mMainThreadRunner.Stop();
    }

    void Join() override {
        mQueueProcessor.Join();
        mMainThreadRunner.Join();
    }

    CRNodeBase* GetNode() const override { return mNode; }

   private:
    CRNodeBase*          mNode;
    queue_processor_t    mQueueProcessor;
    main_thread_runner_t mMainThreadRunner;
};

using CRNodeRoundRobinRunner = CRNodeRunner<CRNodeRoundRobinQueueProcessor, CRNodeMainThreadRunner>;
}  // namespace cris::core
