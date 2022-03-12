#pragma once

#include "cris/core/message/base.h"
#include "cris/core/message/queue.h"
#include "cris/core/node_runner/base.h"
#include "cris/core/node_runner/main_loop_runner.h"
#include "cris/core/node_runner/queue_processor.h"

#include <chrono>
#include <functional>
#include <typeindex>
#include <utility>
#include <vector>

namespace cris::core {

// The nodes are supposed to live throughout the entire message flow. Do not destruct it when
// messages are coming
class CRNodeBase {
   public:
    using channel_subid_t = CRMessageBase::channel_subid_t;
    using channel_id_t    = CRMessageBase::channel_id_t;

    CRNodeBase() = default;

    CRNodeBase(const CRNodeBase&) = delete;

    CRNodeBase(CRNodeBase&&) = delete;

    CRNodeBase& operator=(const CRNodeBase&) = delete;

    virtual ~CRNodeBase() = default;

    virtual void MainLoop(const std::size_t /* thread_idx */, const std::size_t /* thread_num */) {}

    virtual void StopMainLoop() {}

    virtual std::string GetName() const { return "noname"; }

    virtual void Kick() = 0;

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming, nor call it after Node Runner
    // is Running
    template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback);

    void Publish(const channel_subid_t channel_subid, CRMessageBasePtr&& message);

    virtual CRMessageQueue* MessageQueueMapper(const channel_id_t channel) = 0;

    CRMessageQueue* MessageQueueMapper(const CRMessageBasePtr& message);

    static CRNodeBase* GetMessageManager();

   protected:
    virtual void SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) = 0;

    virtual void SubscribeHandler(
        const channel_id_t                             channel,
        std::function<void(const CRMessageBasePtr&)>&& callback) = 0;

    virtual std::vector<CRMessageQueue*> GetNodeQueues() = 0;

    virtual void SetQueueProcessor(CRNodeQueueProcessor* queue_processor) = 0;

    virtual CRNodeQueueProcessor* GetQueueProcessor() = 0;

    void ResetQueueProcessor() { SetQueueProcessor(nullptr); }

    virtual void SetMainLoopRunner(CRNodeMainLoopRunner* main_loop_runner) = 0;

    virtual CRNodeMainLoopRunner* GetMainLoopRunner() = 0;

    void ResetMainLoopRunner() { SetMainLoopRunner(nullptr); }

    friend class CRNodeRunnerBase;
    friend class CRNodeQueueProcessor;
    friend class CRNodeMainLoopRunner;
};

template<class node_t>
concept CRNodeType = std::is_base_of_v<CRNodeBase, node_t>;

template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
void CRNodeBase::Subscribe(const channel_subid_t channel_subid, callback_t&& callback) {
    return SubscribeImpl(
        std::make_pair(std::type_index(typeid(message_t)), channel_subid),
        [callback = std::move(callback)](const CRMessageBasePtr& message) {
            return callback(reinterpret_cast<const std::shared_ptr<message_t>&>(message));
        });
}

}  // namespace cris::core
