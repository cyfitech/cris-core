#include "cris/core/msg/node.h"

#include "cris/core/utils/defs.h"
#include "cris/core/utils/logging.h"

#include <cstdint>
#include <ios>
#include <utility>

namespace cris::core {

CRNode::CRNode(std::string name, std::shared_ptr<JobRunner> runner) : name_(std::move(name)), runner_weak_(runner) {
    if (!runner) {
        LOG(INFO) << __func__ << ": Node \"" << GetName() << "\"(at 0x" << std::hex
                  << reinterpret_cast<std::uintptr_t>(this) << ") binds with no runner." << std::dec;
        return;
    }
    can_subscribe_ = true;
    LOG(INFO) << __func__ << ": Binding node \"" << GetName() << "\"(at 0x" << std::hex
              << reinterpret_cast<std::uintptr_t>(this) << ") to runner at 0x"
              << reinterpret_cast<std::uintptr_t>(runner.get()) << std::dec;
}

CRNode::~CRNode() {
    for (const auto& subscribed : subscribed_) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

bool CRNode::AddMessageToRunner(const CRMessageBasePtr& message) {
    if (auto subscription_info_opt = GetSubscriptionInfo(message)) {
        auto job = [callback = std::move(subscription_info_opt->callback_), message](JobAliveTokenPtr&& token) {
            callback(message, std::move(token));
        };
        return AddJobToRunner(std::move(job), std::move(subscription_info_opt->strand_));
    }
    return false;
}

void CRNode::Publish(const CRNode::channel_subid_t channel_subid, CRMessageBasePtr&& message) {
    message->SetChannelSubId(channel_subid);
    CRMessageBase::Dispatch(message);
}

JobRunnerStrandPtr CRNode::MakeStrand() {
    if (auto runner = runner_weak_.lock()) [[likely]] {
        return runner->MakeStrand();
    } else {
        LOG(WARNING) << __func__ << ": Node \"" << GetName() << "\"(at 0x" << std::hex
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
                   << "is not subscribed by node \"" << GetName() << "\"'(" << this << ").";
        return std::nullopt;
    }
    return callback_search_result->second;
}

void CRNode::SubscribeImpl(
    const channel_id_t                                                 channel,
    std::function<void(const CRMessageBasePtr&, JobAliveTokenPtr&&)>&& callback,
    JobRunnerStrandPtr                                                 strand) {
    auto lck = CRMessageBase::SubscriptionWriteLock();
    CHECK(can_subscribe_) << __func__ << ": Node \"" << GetName() << "\"(at 0x" << std::hex
                          << reinterpret_cast<std::uintptr_t>(this) << ") has not bound with any runner." << std::dec;

    if (!CRMessageBase::SubscribeUnsafe(channel, this, lck)) {
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
                   << "is subscribed. The new callback is ignored. Node: \"" << GetName() << "\"(" << this << ").";
        return;
    }
}

}  // namespace cris::core
