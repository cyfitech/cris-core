#pragma once

#include "cris/core/node_runner/base.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace cris::core {

class CRNodeBase;

class CRNodeQueueProcessor : public CRMultiThreadNodeRunner {
   public:
    using CRMultiThreadNodeRunner::CRMultiThreadNodeRunner;

    ~CRNodeQueueProcessor();

    void Kick();

   protected:
    void WaitForMessage(const std::chrono::nanoseconds timeout);

    void PrepareToRun() override;

    void CleanUp() override;

    std::mutex              wait_message_mutex_;
    std::condition_variable wait_message_cv_;
};

}  // namespace cris::core
