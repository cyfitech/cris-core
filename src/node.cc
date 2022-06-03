#include "cris/core/node.h"

#include "cris/core/logging.h"

namespace cris::core {

CRNode::~CRNode() {
    for (auto&& subscribed : subscribed_) {
        CRMessageBase::Unsubscribe(subscribed, this);
    }
}

bool CRNode::AddMessageToRunner(const CRMessageBasePtr& message) {
    if (auto job_opt = GenerateJob(message)) {
        return AddJobToRunner(std::move(*job_opt));
    };
    return false;
}

bool CRNode::AddJobToRunner(job_t&& job) {
    if (!runner_) {
        LOG(WARNING) << __func__ << ": Node " << GetName() << "(" << this << ") has not bound with a runner yet.";
        return false;
    }
    return runner_->AddJob(std::move(job));
}

void CRNode::Publish(const CRNode::channel_subid_t channel_subid, CRMessageBasePtr&& message) {
    message->SetChannelSubId(channel_subid);
    CRMessageBase::Dispatch(std::move(message));
}

std::optional<CRNode::msg_callback_t> CRNode::GetCallback(const CRMessageBasePtr& message) {
    if (!message) {
        return std::nullopt;
    }

    const auto channel = message->GetChannelId();

    auto callback_search_result = callbacks_.find(channel);
    if (callback_search_result == callbacks_.end()) {
        LOG(ERROR) << __func__ << ": message channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is not subscribed by node " << GetName() << "(" << this << ").";
        return std::nullopt;
    }
    return callback_search_result->second;
}

std::optional<CRNode::job_t> CRNode::GenerateJob(const CRMessageBasePtr& message) {
    if (auto callback_opt = GetCallback(message)) {
        return [callback = std::move(*callback_opt), message]() {
            callback(message);
        };
    }
    return std::nullopt;
}

void CRNode::SubscribeImpl(const channel_id_t channel, std::function<void(const CRMessageBasePtr&)>&& callback) {
    if (!CRMessageBase::Subscribe(channel, this)) {
        return;
    }
    subscribed_.push_back(channel);
    auto callback_insert = callbacks_.emplace(channel, std::move(callback));
    if (!callback_insert.second) {
        LOG(ERROR) << __func__ << ": channel (" << channel.first.name() << ", " << channel.second << ") "
                   << "is subscribed. The new callback is ignored. Node: " << GetName() << "(" << this << ").";
        return;
    }
}

}  // namespace cris::core
