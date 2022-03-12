#pragma once

#include "cris/core/message/queue.h"
#include "cris/core/node/base.h"
#include "cris/core/node_runner/main_loop_runner.h"
#include "cris/core/node_runner/queue_processor.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <typeindex>

namespace cris::core {

class CRNode : public CRNodeBase {
   public:
    virtual ~CRNode();

   private:
    void SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) override;

    void Kick() override;

    void SetQueueProcessor(CRNodeQueueProcessor* queue_processor) override;

    CRNodeQueueProcessor* GetQueueProcessor() override;

    void SetMainLoopRunner(CRNodeMainLoopRunner* main_loop_runner) override;

    CRNodeMainLoopRunner* GetMainLoopRunner() override;

    std::vector<channel_id_t> subscribed_;
    std::mutex                wait_message_mutex_;
    std::condition_variable   wait_message_cv_;
    CRNodeMainLoopRunner*     main_loop_runner_{nullptr};
    CRNodeQueueProcessor*     queue_processor_{nullptr};
};

}  // namespace cris::core
