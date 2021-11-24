#include "cris/core/message/base.h"
#include "cris/core/node/base.h"

#include <glog/logging.h>

#include <map>

namespace cris::core {

std::map<std::string, CRMessageBase::SubscriptionInfo>* CRMessageBase::GetSubscriptionMap() {
    static std::map<std::string, SubscriptionInfo> subscription_map;
    return &subscription_map;
};

void CRMessageBase::Dispatch(const CRMessageBasePtr& message) {
    auto* subscription_info = GetSubscriptionInfo(message->GetMessageTypeName());
    if (!subscription_info) {
        return;
    }
    for (auto&& node : subscription_info->sub_list_) {
        auto* queue = node->MessageQueueMapper(message);
        if (!queue) [[unlikely]] {
            LOG(ERROR) << __func__ << ": node: " << node << ", no queue for message " << message->GetMessageTypeName()
                       << ", skipping";
            continue;
        }
        queue->AddMessage(CRMessageBasePtr(message));
        node->Kick();
    }
    subscription_info->latest_delivered_time_.store(GetSystemTimestampNsec());
}

bool CRMessageBase::Subscribe(const std::string& message_type, CRNodeBase* node) {
    auto& subscription_list = (*GetSubscriptionMap())[message_type].sub_list_;

    if (std::find(subscription_list.begin(), subscription_list.end(), node) != subscription_list.end()) {
        LOG(WARNING) << __func__ << ": Message type '" << message_type << " is subscribed by the node " << node
                     << ", skipping subscription.";
        return false;
    }
    subscription_list.emplace_back(node);
    return true;
}

void CRMessageBase::Unsubscribe(const std::string& message_type, CRNodeBase* node) {
    auto* subscription_map        = GetSubscriptionMap();
    auto  subscription_map_search = subscription_map->find(message_type);
    if (subscription_map_search == subscription_map->end()) {
        LOG(WARNING) << __func__ << ": message '" << message_type << "' is unknown.";
        return;
    }

    if (!std::erase(subscription_map_search->second.sub_list_, node)) {
        LOG(WARNING) << __func__ << ": message '" << message_type << "' is not subscribed by node " << node;
    }
}

cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTimeImpl(const std::string& message_type) {
    constexpr cr_timestamp_nsec_t kDefaultDeliveredTime = 0;

    auto* subscription_map        = GetSubscriptionMap();
    auto  subscription_map_search = subscription_map->find(message_type);
    if (subscription_map_search == subscription_map->end()) {
        LOG(WARNING) << __func__ << ": message '" << message_type << "' is unknown.";
        return kDefaultDeliveredTime;
    }
    return subscription_map_search->second.latest_delivered_time_.load();
}

CRMessageBase::SubscriptionInfo* CRMessageBase::GetSubscriptionInfo(const std::string& message_type) {
    auto* subscription_map  = GetSubscriptionMap();
    auto  subscription_find = subscription_map->find(message_type);
    if (subscription_find == subscription_map->end()) {
        return nullptr;
    }
    return &subscription_find->second;
}

}  // namespace cris::core
