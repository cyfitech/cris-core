#pragma once

#include "cris/core/node_runner/base.h"

#include <cstddef>
#include <functional>

namespace cris::core {

class CRNodeMainLoopRunner : public CRMultiThreadNodeRunner {
   public:
    CRNodeMainLoopRunner(CRNodeBase* node, std::size_t thread_num);

    ~CRNodeMainLoopRunner();

    CRNodeBase* GetNode();

   private:
    void PrepareToRun() override;

    void CleanUp() override;

    std::function<void()> GetWorker(std::size_t thread_idx, std::size_t thread_num) override;

    void NotifyWorkersToStop() override;

};

}  // namespace cris::core
