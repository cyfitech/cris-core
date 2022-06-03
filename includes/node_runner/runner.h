#pragma once

#include "cris/core/node.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace cris::core {

class CRNodeRunnerBase {
   public:
    CRNodeRunnerBase() = default;

    CRNodeRunnerBase(const CRNodeRunnerBase&) = delete;

    CRNodeRunnerBase(CRNodeRunnerBase&&) = default;

    CRNodeRunnerBase& operator=(const CRNodeRunnerBase&) = delete;

    virtual ~CRNodeRunnerBase() = default;

    virtual CRNode* GetNode() const = 0;

    std::vector<CRMessageQueue*> GetNodeQueues();

    virtual void Run() = 0;

    virtual void Stop() = 0;

    virtual void Join() = 0;
};

class CRMultiThreadNodeRunner : public CRNodeRunnerBase {
   public:
    CRMultiThreadNodeRunner(CRNode* node, std::size_t thread_num);

    ~CRMultiThreadNodeRunner();

    CRNode* GetNode() const override;

    std::size_t GetThreadNum() const;

    void Run() override;

    void Stop() override;

    void Join() override;

    virtual void PrepareToRun() = 0;

    virtual std::function<void()> GetWorker(std::size_t thread_idx, std::size_t thread_num) = 0;

    virtual void NotifyWorkersToStop() = 0;

   private:
    CRNode*                  node_;
    std::size_t              thread_num_;
    std::vector<std::thread> worker_threads_;
    bool                     is_running_{false};
    std::mutex               run_state_mutex_;
    std::mutex               run_threads_mutex_;
};

class CRNodeMainThreadRunner : public CRMultiThreadNodeRunner {
   public:
    CRNodeMainThreadRunner(CRNode* node, std::size_t thread_num, bool auto_run = false);

    ~CRNodeMainThreadRunner();

    void PrepareToRun() override;

    std::function<void()> GetWorker(std::size_t thread_idx, std::size_t thread_num) override;

    void NotifyWorkersToStop() override;
};

class CRNodeRoundRobinQueueProcessor : public CRMultiThreadNodeRunner {
   public:
    CRNodeRoundRobinQueueProcessor(CRNode* node, std::size_t thread_num, bool auto_run = false);

    ~CRNodeRoundRobinQueueProcessor();

    void PrepareToRun() override;

    std::function<void()> GetWorker(std::size_t thread_idx, std::size_t thread_num) override;

    void NotifyWorkersToStop() override;

   private:
    void WorkerLoop(const std::size_t thread_idx, const std::size_t thread_num);

    std::vector<CRMessageQueue*> node_queues_;
    std::atomic<bool>            shutdown_flag_{false};
};

template<class runner_t>
concept CRNodeRunnerType = std::is_base_of_v<CRNodeRunnerBase, runner_t>;

template<CRNodeRunnerType queue_processor_t, CRNodeRunnerType main_thread_runner_t = CRNodeMainThreadRunner>
class CRNodeRunner : public CRNodeRunnerBase {
   public:
    CRNodeRunner(
        CRNode*     node,
        std::size_t queue_processor_thread_num,
        std::size_t main_thread_num = 1,
        bool        auto_run        = false)
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

    CRNode* GetNode() const override { return node_; }

   private:
    CRNode*              node_;
    queue_processor_t    queue_processor_;
    main_thread_runner_t main_thread_runner_;
};

using CRNodeRoundRobinRunner = CRNodeRunner<CRNodeRoundRobinQueueProcessor, CRNodeMainThreadRunner>;
}  // namespace cris::core
