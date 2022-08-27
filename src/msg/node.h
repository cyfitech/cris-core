#pragma once

#include "cris/core/msg/message.h"
#include "cris/core/sched/job_runner.h"
#include "cris/core/utils/logging.h"

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

template<class callback_t, class message_t = CRMessageBase>
concept CRSingleMessageCallbackType = std::is_base_of_v<CRMessageBase, message_t> &&
    std::is_void_v<decltype(std::declval<callback_t>()(std::declval<const std::shared_ptr<message_t>&>()))>;

template<class callback_t, class message_t = CRMessageBase>
concept CRMessageWithAliveTokenCallbackType =
    std::is_base_of_v<CRMessageBase, message_t> && std::is_void_v<decltype(std::declval<callback_t>()(
        std::declval<const std::shared_ptr<message_t>&>(),
        std::declval<JobAliveTokenPtr>()))>;

template<class callback_t, class message_t = CRMessageBase>
concept CRMessageCallbackType =
    CRSingleMessageCallbackType<callback_t, message_t> || CRMessageWithAliveTokenCallbackType<callback_t, message_t>;

class CRNode {
   public:
    using channel_subid_t = CRMessageBase::channel_subid_t;
    using channel_id_t    = CRMessageBase::channel_id_t;
    using job_t           = JobRunner::job_t;

    explicit CRNode() : CRNode("noname", nullptr) {}

    explicit CRNode(std::string name) : CRNode(std::move(name), nullptr) {}

    explicit CRNode(std::shared_ptr<JobRunner> runner) : CRNode("noname", std::move(runner)) {}

    explicit CRNode(std::string name, std::shared_ptr<JobRunner> runner);

    virtual ~CRNode();

    // Some long lasting works that are not event-driven.
    virtual void MainLoop() {}

    virtual void StopMainLoop() {}

    std::string GetName() const { return name_; }

    template<class strand_job_t>
    bool AddJobToRunner(strand_job_t&& job, JobRunnerStrandPtr strand);

    bool AddJobToRunner(job_t&& job) { return AddJobToRunner(std::move(job), nullptr); }

    bool AddMessageToRunner(const CRMessageBasePtr& message);

    template<CRMessageType message_t, CRSingleMessageCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback, JobRunnerStrandPtr strand);

    template<CRMessageType message_t, CRMessageWithAliveTokenCallbackType<message_t> callback_t>
    void Subscribe(const channel_subid_t channel_subid, callback_t&& callback, JobRunnerStrandPtr strand);

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
        std::function<void(const CRMessageBasePtr&, JobAliveTokenPtr&&)> callback_;
        JobRunnerStrandPtr                                               strand_;
    };

    using callback_map_t = std::unordered_map<channel_id_t, SubscriptionInfo, boost::hash<channel_id_t>>;

    JobRunnerStrandPtr MakeStrand();

    std::optional<SubscriptionInfo> GetSubscriptionInfo(const CRMessageBasePtr& message);

    void SubscribeImpl(
        const channel_id_t                                                 channel,
        std::function<void(const CRMessageBasePtr&, JobAliveTokenPtr&&)>&& callback,
        JobRunnerStrandPtr                                                 strand);

    std::string               name_;
    bool                      can_subscribe_{false};
    std::vector<channel_id_t> subscribed_;
    callback_map_t            callbacks_;
    std::weak_ptr<JobRunner>  runner_weak_;
};

template<class node_t>
concept CRNodeType = std::is_base_of_v<CRNode, node_t>;

template<class strand_job_t>
bool CRNode::AddJobToRunner(strand_job_t&& job, JobRunnerStrandPtr strand) {
    if (auto runner = runner_weak_.lock()) [[likely]] {
        return runner->AddJob(std::forward<strand_job_t>(job), std::move(strand));
    } else {
        LOG(ERROR) << __func__ << ": Node \"" << GetName() << "\"(at 0x" << std::hex
                   << reinterpret_cast<std::uintptr_t>(this) << ") has not bound with any runner." << std::dec;
        return false;
    }
}

template<CRMessageType message_t, CRMessageWithAliveTokenCallbackType<message_t> callback_t>
void CRNode::Subscribe(const channel_subid_t channel_subid, callback_t&& callback, JobRunnerStrandPtr strand) {
    return SubscribeImpl(
        std::make_pair(std::type_index(typeid(message_t)), channel_subid),
        [callback = std::move(callback)](const CRMessageBasePtr& message, JobAliveTokenPtr&& token) {
            callback(reinterpret_cast<const std::shared_ptr<message_t>&>(message), std::move(token));
        },
        std::move(strand));
}

template<CRMessageType message_t, CRSingleMessageCallbackType<message_t> callback_t>
void CRNode::Subscribe(const channel_subid_t channel_subid, callback_t&& callback, JobRunnerStrandPtr strand) {
    return SubscribeImpl(
        std::make_pair(std::type_index(typeid(message_t)), channel_subid),
        [callback = std::move(callback)](const CRMessageBasePtr& message, JobAliveTokenPtr&&) {
            callback(reinterpret_cast<const std::shared_ptr<message_t>&>(message));
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
