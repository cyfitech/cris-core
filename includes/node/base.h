#pragma once

#include "cris/core/message/base.h"
#include "cris/core/message/queue.h"

#include <chrono>
#include <typeindex>
#include <utility>

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

    template<class duration_t>
    void WaitForMessage(duration_t&& timeout);

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
    virtual void WaitForMessageImpl(std::chrono::nanoseconds timeout) = 0;

    virtual void SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) = 0;

    virtual void SubscribeHandler(
        const channel_id_t                             channel,
        std::function<void(const CRMessageBasePtr&)>&& callback) = 0;

    virtual std::vector<CRMessageQueue*> GetNodeQueues() = 0;

    friend class CRNodeRunnerBase;
};

template<class node_t>
concept CRNodeType = std::is_base_of_v<CRNodeBase, node_t>;

template<class duration_t>
void CRNodeBase::WaitForMessage(duration_t&& timeout) {
    return WaitForMessageImpl(std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(timeout)));
}

template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
void CRNodeBase::Subscribe(const channel_subid_t channel_subid, callback_t&& callback) {
    return SubscribeImpl(
        std::make_pair(std::type_index(typeid(message_t)), channel_subid),
        [callback = std::move(callback)](const CRMessageBasePtr& message) {
            return callback(reinterpret_cast<const std::shared_ptr<message_t>&>(message));
        });
}

}  // namespace cris::core
