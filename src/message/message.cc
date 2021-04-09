#include <glog/logging.h>

#include <map>

#include "cris/core/message/base.h"
#include "cris/core/node/base.h"

namespace cris::core {

namespace impl {

std::map<std::string, CRMessageBase::subscription_list_t> subscription_map{};

}  // namespace impl

void CRMessageBase::Dispatch(const CRMessageBasePtr& message) {
    auto* subscription_list = GetSubscriptionList(message->GetMessageTypeName());
    if (!subscription_list) {
        return;
    }
    for (auto&& node : *subscription_list) {
        auto* queue = node->MessageQueueMapper(message);
        if (!queue) {
            LOG(ERROR) << __func__ << ": node: " << node << ", no queue for message "
                       << message->GetMessageTypeName() << ", skipping";
            continue;
        }
        queue->AddMessage(CRMessageBasePtr(message));
        node->Kick();
    }
}

void CRMessageBase::Subscribe(const std::string& message_type, CRNodeBase* node) {
    impl::subscription_map[message_type].emplace_back(node);
}

void CRMessageBase::Unsubscribe(const std::string& message_type, CRNodeBase* node) {
    auto subscription_map_search = impl::subscription_map.find(message_type);
    if (subscription_map_search == impl::subscription_map.end()) {
        LOG(WARNING) << __func__ << ": message '" << message_type << "' is unknown.";
        return;
    }
    auto& subscription_list = subscription_map_search->second;
    auto  subscription_list_search =
        std::find(subscription_list.begin(), subscription_list.end(), node);
    if (subscription_list_search == subscription_list.end()) {
        LOG(WARNING) << __func__ << ": message '" << message_type << "' is not subscribed by node "
                     << node;
        return;
    }
    std::swap(*subscription_list_search, subscription_list.back());
    subscription_list.pop_back();
}

const CRMessageBase::subscription_list_t* CRMessageBase::GetSubscriptionList(
    const std::string& message_type) {
    auto subscription_find = impl::subscription_map.find(message_type);
    if (subscription_find == impl::subscription_map.end()) {
        return nullptr;
    }
    return &subscription_find->second;
}

}  // namespace cris::core
