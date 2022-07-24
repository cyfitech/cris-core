#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/sched/job_runner.h"

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
    using strand_ptr_t    = std::shared_ptr<JobRunnerStrand>;

    explicit CRNode() : CRNode("noname", nullptr) {}

    explicit CRNode(std::string name) : CRNode(std::move(name), nullptr) {}

    explicit CRNode(std::shared_ptr<JobRunner> runner) : CRNode("noname", std::move(runner)) {}

    explicit CRNode(std::string name, std::shared_ptr<JobRunner> runner);

    virtual ~CRNode();

    // Some long lasting works that are not event-driven.
    virtual void MainLoop() {}

    virtual void StopMainLoop() {}

    std::string GetName() const { return name_; }

    bool AddJobToRunner(job_t&& job, strand_ptr_t strand);

    bool AddJobToRunner(job_t&& job) { return AddJobToRunner(std::move(job), nullptr); }

    bool AddMessageToRunner(const CRMessageBasePtr& message);

    template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback, strand_ptr_t strand);

    template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback, const bool allow_concurrency) {
        Subscribe<message_t>(
            channel_subid,
            std::forward<callback_t>(callback),
            allow_concurrency ? nullptr : MakeStrand());
    }

    template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback) {
        return Subscribe<message_t>(channel_subid, std::forward<callback_t>(callback), /* allow_concurrency = */ true);
    }

    void Publish(const channel_subid_t channel_subid, CRMessageBasePtr&& message);

   protected:
    struct SubscriptionInfo {
        msg_callback_t callback_;
        strand_ptr_t   strand_;
    };

    using callback_map_t = std::unordered_map<channel_id_t, SubscriptionInfo, boost::hash<channel_id_t>>;

    strand_ptr_t MakeStrand();

    std::optional<SubscriptionInfo> GetSubscriptionInfo(const CRMessageBasePtr& message);

    void SubscribeImpl(
        const channel_id_t                             channel,
        std::function<void(const CRMessageBasePtr&)>&& callback,
        strand_ptr_t                                   strand);

    std::string               name_;
    bool                      can_subscribe_{false};
    std::vector<channel_id_t> subscribed_;
    callback_map_t            callbacks_;
    std::weak_ptr<JobRunner>  runner_weak_;
};

template<class node_t>
concept CRNodeType = std::is_base_of_v<CRNode, node_t>;

template<CRMessageType message_t, CRMessageCallbackType<message_t> callback_t>
void CRNode::Subscribe(const channel_subid_t channel_subid, callback_t&& callback, strand_ptr_t strand) {
    return SubscribeImpl(
        std::make_pair(std::type_index(typeid(message_t)), channel_subid),
        [callback = std::move(callback)](const CRMessageBasePtr& message) {
            return callback(reinterpret_cast<const std::shared_ptr<message_t>&>(message));
        },
        std::move(strand));
}

template<class node_t, CRNodeType base_t = CRNode>
class CRNamedNode : public base_t {
   public:
    // Forward constructor parameters to base_t
    template<class... args_t>
    CRNamedNode(args_t&&... args) : base_t(GetTypeName<node_t>(), std::forward<args_t>(args)...) {}
};

}  // namespace cris::core
