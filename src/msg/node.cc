#include "cris/core/msg/node.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <cstdint>
#include <ios>
#include <utility>

namespace cris::core {

CRNode::~CRNode() {
    for (const auto& subscribed : subscribed_) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

void CRNode::SetRunner(std::shared_ptr<JobRunner> runner) {
    LOG(INFO) << __func__ << ": Binding node " << GetName() << "(at 0x" << std::hex
              << reinterpret_cast<std::uintptr_t>(this) << ") to runner at 0x"
              << reinterpret_cast<std::uintptr_t>(runner.get()) << std::dec;
    runner_weak_ = runner;
}

bool CRNode::AddMessageToRunner(const CRMessageBasePtr& message) {
    if (auto subscription_info_opt = GetSubscriptionInfo(message)) {
        auto job = [callback = std::move(subscription_info_opt->callback_), message]() {
            callback(message);
        };
        return AddJobToRunner(std::move(job), std::move(subscription_info_opt->strand_));
    }
    return false;
}

bool CRNode::AddJobToRunner(job_t&& job, strand_ptr_t strand) {
    if (auto runner = runner_weak_.lock()) [[likely]] {
        return runner->AddJob(std::move(job), std::move(strand));
    } else {
        LOG(WARNING) << __func__ << ": Node " << GetName() << "(at 0x" << std::hex
                     << reinterpret_cast<std::uintptr_t>(this) << ") has not bound with a runner yet." << std::dec;
        return false;
    }
}

void CRNode::Publish(const CRNode::channel_subid_t channel_subid, CRMessageBasePtr&& message) {
    message->SetChannelSubId(channel_subid);
    CRMessageBase::Dispatch(std::move(message));
}

CRNode::strand_ptr_t CRNode::MakeStrand() {
    if (auto runner = runner_weak_.lock()) [[likely]] {
        return runner->MakeStrand();
    } else {
        LOG(WARNING) << __func__ << ": Node " << GetName() << "(at 0x" << std::hex
                     << reinterpret_cast<std::uintptr_t>(this) << ") has not bound with a runner yet." << std::dec;
        return nullptr;
    }
}

std::optional<CRNode::SubscriptionInfo> CRNode::GetSubscriptionInfo(const CRMessageBasePtr& message) {
    if (!message) {
        return std::nullopt;
    }

    const auto channel = message->GetChannelId();

    const auto callback_search_result = callbacks_.find(channel);
    if (callback_search_result == callbacks_.end()) {
        LOG(ERROR) << __func__ << ": message channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is not subscribed by node " << GetName() << "(" << this << ").";
        return std::nullopt;
    }
    return callback_search_result->second;
}

void CRNode::SubscribeImpl(
    const channel_id_t                             channel,
    std::function<void(const CRMessageBasePtr&)>&& callback,
    strand_ptr_t                                   strand) {
    if (!CRMessageBase::Subscribe(channel, this)) {
        return;
    }
    subscribed_.push_back(channel);
    auto callback_insert = callbacks_.emplace(
        channel,
        SubscriptionInfo{
            .callback_ = std::move(callback),
            .strand_   = std::move(strand),
        });
    if (!callback_insert.second) {
        LOG(ERROR) << __func__ << ": channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is subscribed. The new callback is ignored. Node: " << GetName() << "(" << this << ").";
        return;
    }
}

}  // namespace cris::core
