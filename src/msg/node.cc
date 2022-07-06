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
    if (auto job_opt = GenerateJob(message)) {
        return AddJobToRunner(std::move(*job_opt));
    };
    return false;
}

bool CRNode::AddJobToRunner(job_t&& job) {
    if (auto runner = runner_weak_.lock()) [[likely]] {
        return runner->AddJob(std::move(job));
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

std::optional<CRNode::msg_callback_t> CRNode::GetCallback(const CRMessageBasePtr& message) {
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
