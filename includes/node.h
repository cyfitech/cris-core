#pragma once

#include "cris/core/job_runner.h"
#include "cris/core/message.h"

#include <boost/functional/hash.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cris::core {

class CRNode {
   public:
    using channel_subid_t = CRMessageBase::channel_subid_t;
    using channel_id_t    = CRMessageBase::channel_id_t;
    using msg_callback_t  = std::function<void(const CRMessageBasePtr&)>;
    using job_t           = JobRunner::job_t;

    explicit CRNode() : CRNode("noname") {}

    explicit CRNode(std::string name) : name_(std::move(name)) {}

    virtual ~CRNode();

    // Some long lasting works that are not event-driven.
    virtual void MainLoop() {}

    virtual void StopMainLoop() {}

    std::string GetName() const { return name_; }

    void SetRunner(JobRunner* runner) { runner_ = runner; }

    bool AddJobToRunner(job_t&& job);

    bool AddMessageToRunner(const CRMessageBasePtr& message);

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming, nor call it after runner
    // is running
    template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback);

    void Publish(const channel_subid_t channel_subid, CRMessageBasePtr&& message);

   protected:
    using callback_map_t = std::unordered_map<channel_id_t, msg_callback_t, boost::hash<channel_id_t>>;

    std::optional<msg_callback_t> GetCallback(const CRMessageBasePtr& message);

    std::optional<job_t> GenerateJob(const CRMessageBasePtr& message);

    void SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback);

    std::string                name_;
    std::vector<channel_id_t>  subscribed_;
    callback_map_t             callbacks_;
    JobRunner*                 runner_{nullptr};
};

template<class node_t>
concept CRNodeType = std::is_base_of_v<CRNode, node_t>;

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
