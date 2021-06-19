#pragma once

#include <chrono>

#include "cris/core/message/queue.h"

namespace cris::core {

// The nodes are supposed to live throughout the entire message flow. Do not destruct it when
// messages are coming
class CRNodeBase {
   public:
    CRNodeBase() = default;

    CRNodeBase(const CRNodeBase &) = delete;

    CRNodeBase(CRNodeBase &&) = delete;

    CRNodeBase &operator=(const CRNodeBase &) = delete;

    virtual ~CRNodeBase() = default;

    virtual void MainLoop(const size_t thread_idx, const size_t thread_num) {}

    virtual void StopMainLoop() {}

    virtual std::string GetName() const { return "noname"; }

    template<class duration_t>
    void WaitForMessage(duration_t &&timeout);

    virtual void Kick() = 0;

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming, nor call it after Node Runner
    // is Running
    template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
    void Subscribe(callback_t &&callback);

    void Publish(CRMessageBasePtr &&message);

    virtual CRMessageQueue *MessageQueueMapper(const CRMessageBasePtr &message) = 0;

    static CRNodeBase *GetMessageManager();

   protected:
    virtual void WaitForMessageImpl(std::chrono::nanoseconds timeout) = 0;

    virtual void SubscribeImpl(std::string &&                                  message_name,
                               std::function<void(const CRMessageBasePtr &)> &&callback) = 0;

    virtual void SubscribeHandler(std::string &&                                  message_name,
                                  std::function<void(const CRMessageBasePtr &)> &&callback) = 0;

    virtual std::vector<CRMessageQueue *> GetNodeQueues() = 0;

    friend class CRNodeRunnerBase;
};

template<class node_t>
concept CRNodeType = std::is_base_of_v<CRNodeBase, node_t>;

template<class duration_t>
void CRNodeBase::WaitForMessage(duration_t &&timeout) {
    return WaitForMessageImpl(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(timeout)));
}

template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
void CRNodeBase::Subscribe(callback_t &&callback) {
    return SubscribeImpl(
        CRMessageBase::GetMessageTypeName<message_t>(),
        [callback = std::move(callback)](const CRMessageBasePtr &message) {
            return callback(reinterpret_cast<const std::shared_ptr<message_t> &>(message));
        });
}

}  // namespace cris::core
