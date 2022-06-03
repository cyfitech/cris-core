#pragma once

#include "cris/core/message/lock_queue.h"
#include "cris/core/message/queue.h"

#include <boost/functional/hash.hpp>

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <typeindex>
#include <vector>

namespace cris::core {

class CRNode {
   public:
    using channel_subid_t = CRMessageBase::channel_subid_t;
    using channel_id_t    = CRMessageBase::channel_id_t;

    explicit CRNode(std::size_t queue_capacity) : CRNode("noname", queue_capacity) {}

    explicit CRNode(std::string name, std::size_t queue_capacity)
        : name_(std::move(name))
        , queue_capacity_(queue_capacity) {}

    virtual ~CRNode();

    virtual void MainLoop(const std::size_t /* thread_idx */, const std::size_t /* thread_num */) {}

    virtual void StopMainLoop() {}

    std::string GetName() const { return name_; }

    void Kick();

    template<class duration_t>
    void WaitForMessage(duration_t&& timeout);

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming, nor call it after Node Runner
    // is Running
    template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback);

    void Publish(const channel_subid_t channel_subid, CRMessageBasePtr&& message);

    CRMessageQueue* MessageQueueMapper(const channel_id_t channel);

    CRMessageQueue* MessageQueueMapper(const CRMessageBasePtr& message);

   protected:
    using queue_callback_t = std::function<void(const CRMessageBasePtr&)>;
    using queue_map_t = std::unordered_map<channel_id_t, std::unique_ptr<CRMessageQueue>, boost::hash<channel_id_t>>;

    void WaitForMessageImpl(std::chrono::nanoseconds timeout);

    void SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback);

    void SubscribeHandler(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback);

    std::vector<CRMessageQueue*> GetNodeQueues();

    // TODO(hao.chen): Message queue is deprecating, we add this for compatibility for now.
    std::unique_ptr<CRMessageQueue> MakeMessageQueue(queue_callback_t&& callback) {
        return std::make_unique<CRMessageLockQueue>(queue_capacity_, this, std::move(callback));
    }

    friend class CRNodeRunnerBase;

    std::string               name_;
    std::vector<channel_id_t> subscribed_;
    std::mutex                wait_message_mutex_;
    std::condition_variable   wait_message_cv_;
    std::size_t               queue_capacity_;
    queue_map_t               queues_;
};

template<class node_t>
concept CRNodeType = std::is_base_of_v<CRNode, node_t>;

template<class duration_t>
void CRNode::WaitForMessage(duration_t&& timeout) {
    return WaitForMessageImpl(std::chrono::duration_cast<std::chrono::nanoseconds>(std::forward<duration_t>(timeout)));
}

template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
void CRNode::Subscribe(const channel_subid_t channel_subid, callback_t&& callback) {
    return SubscribeImpl(
        std::make_pair(std::type_index(typeid(message_t)), channel_subid),
        [callback = std::move(callback)](const CRMessageBasePtr& message) {
            return callback(reinterpret_cast<const std::shared_ptr<message_t>&>(message));
        });
}

template<class node_t, CRNodeType base_t = CRNode>
class CRNamedNode : public base_t {
   public:
    // Forward constructor parameters to base_t
    template<class... args_t>
    CRNamedNode(args_t&&... args) : base_t(GetTypeName<node_t>(), std::forward<args_t>(args)...) {}
};

}  // namespace cris::core
