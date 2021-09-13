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
    CRNodeBase*              node_;
    size_t                   thread_num_;
    std::vector<std::thread> worker_threads_;
    bool                     is_running_{false};
    std::mutex               run_state_mutex_;
    std::mutex               run_threads_mutex_;
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

    std::vector<CRMessageQueue*> node_queues_;
    std::atomic<bool>            shutdown_flag_{false};
};

template<class runner_t>
concept CRNodeRunnerType = std::is_base_of_v<CRNodeRunnerBase, runner_t>;

template<CRNodeRunnerType queue_processor_t, CRNodeRunnerType main_thread_runner_t = CRNodeMainThreadRunner>
class CRNodeRunner : public CRNodeRunnerBase {
   public:
    CRNodeRunner(CRNodeBase* node, size_t queue_processor_thread_num, size_t main_thread_num = 1, bool auto_run = false)
        : node_(node)
        , queue_processor_(node, queue_processor_thread_num, auto_run)
        , main_thread_runner_(node, main_thread_num, auto_run) {}

    void Run() override {
        queue_processor_.Run();
        main_thread_runner_.Run();
    }

    void Stop() override {
        queue_processor_.Stop();
        main_thread_runner_.Stop();
    }

    void Join() override {
        queue_processor_.Join();
        main_thread_runner_.Join();
    }

    CRNodeBase* GetNode() const override { return node_; }

   private:
    CRNodeBase*          node_;
    queue_processor_t    queue_processor_;
    main_thread_runner_t main_thread_runner_;
};

using CRNodeRoundRobinRunner = CRNodeRunner<CRNodeRoundRobinQueueProcessor, CRNodeMainThreadRunner>;
}  // namespace cris::core
