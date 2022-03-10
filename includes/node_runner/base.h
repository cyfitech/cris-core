#pragma once

#include "cris/core/message/queue.h"

#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace cris::core {

class CRNodeBase;

class CRNodeRunnerBase {
   public:
    CRNodeRunnerBase() = default;

    CRNodeRunnerBase(const CRNodeRunnerBase&) = delete;

    CRNodeRunnerBase(CRNodeRunnerBase&&) = default;

    CRNodeRunnerBase& operator=(const CRNodeRunnerBase&) = delete;

    virtual ~CRNodeRunnerBase() = default;

    virtual const std::vector<CRNodeBase*>& GetNodes() const = 0;

    virtual void Run() = 0;

    virtual void Stop() = 0;

    virtual void Join() = 0;

   protected:
    std::vector<CRMessageQueue*> GetNodeQueues(CRNodeBase* node);
};

class CRMultiThreadNodeRunner : public CRNodeRunnerBase {
   public:
    CRMultiThreadNodeRunner(const std::vector<CRNodeBase*>& nodes, std::size_t thread_num);

    const std::vector<CRNodeBase*>& GetNodes() const override;

    std::size_t GetThreadNum() const;

    void Run() override;

    void Stop() override;

    void Join() override;

   private:
    virtual std::function<void()> GetWorker(std::size_t thread_idx, std::size_t thread_num) = 0;

    virtual void PrepareToRun() = 0;

    virtual void CleanUp() = 0;

    virtual void NotifyWorkersToStop() = 0;

    std::vector<CRNodeBase*> nodes_;
    std::size_t              thread_num_;
    std::vector<std::thread> worker_threads_;
    bool                     is_running_{false};
    std::mutex               run_state_mutex_;
    std::mutex               run_threads_mutex_;
};

}  // namespace cris::core
