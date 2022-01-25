#include "cris/core/message/base.h"
#include "cris/core/node/base.h"

#include <glog/logging.h>

#include <boost/functional/hash.hpp>

#include <typeindex>
#include <unordered_map>
#include <utility>

namespace cris::core {

namespace {

class SubscriptionInfo {
   public:
    std::atomic<cr_timestamp_nsec_t> latest_delivered_time_{0};
    std::vector<CRNodeBase*>         sub_list_;
};

}  // namespace

using channel_id_t = CRMessageBase::channel_id_t;

// Mapping from type info and channel ID to subscription info
// TODO (hao.chen): add channel ID support. Currently they are all 0.
using subscription_key_t = std::pair<std::type_index, channel_id_t>;
using subscription_map_t = std::unordered_map<subscription_key_t, SubscriptionInfo, boost::hash<subscription_key_t>>;

constexpr static channel_id_t kDefaultChannelID = 0;

static subscription_map_t& GetSubscriptionMap() {
    static subscription_map_t subscription_map;
    return subscription_map;
};

static SubscriptionInfo* GetSubscriptionInfo(const std::type_index message_type) {
    auto& subscription_map  = GetSubscriptionMap();
    auto  subscription_find = subscription_map.find(std::make_pair(message_type, kDefaultChannelID));
    if (subscription_find == subscription_map.end()) {
        return nullptr;
    }
    return &subscription_find->second;
}

void CRMessageBase::Dispatch(const CRMessageBasePtr& message) {
    auto* subscription_info = GetSubscriptionInfo(message->GetMessageTypeIndex());
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

bool CRMessageBase::Subscribe(const std::type_index message_type, CRNodeBase* node) {
    auto& subscription_list = GetSubscriptionMap()[std::make_pair(message_type, kDefaultChannelID)].sub_list_;

    if (std::find(subscription_list.begin(), subscription_list.end(), node) != subscription_list.end()) {
        LOG(WARNING) << __func__ << ": Message type '" << message_type.name() << " is subscribed by the node " << node
                     << ", skipping subscription.";
        return false;
    }
    subscription_list.emplace_back(node);
    return true;
}

void CRMessageBase::Unsubscribe(const std::type_index message_type, CRNodeBase* node) {
    auto& subscription_map        = GetSubscriptionMap();
    auto  subscription_map_search = subscription_map.find(std::make_pair(message_type, kDefaultChannelID));
    if (subscription_map_search == subscription_map.end()) {
        LOG(WARNING) << __func__ << ": message '" << message_type.name() << "' is unknown.";
        return;
    }

    if (!std::erase(subscription_map_search->second.sub_list_, node)) {
        LOG(WARNING) << __func__ << ": message '" << message_type.name() << "' is not subscribed by node " << node;
    }
}

cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime(const std::type_index message_type) {
    constexpr cr_timestamp_nsec_t kDefaultDeliveredTime = 0;

    auto& subscription_map        = GetSubscriptionMap();
    auto  subscription_map_search = subscription_map.find(std::make_pair(message_type, kDefaultChannelID));
    if (subscription_map_search == subscription_map.end()) {
        LOG(WARNING) << __func__ << ": message '" << message_type.name() << "' is unknown.";
        return kDefaultDeliveredTime;
    }
    return subscription_map_search->second.latest_delivered_time_.load();
}

}  // namespace cris::core
