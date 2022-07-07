#include "cris/core/msg/message.h"

#include "cris/core/msg/node.h"
#include "cris/core/utils/logging.h"

#include <boost/functional/hash.hpp>

#include <typeindex>
#include <unordered_map>
#include <utility>

namespace cris::core {

namespace {

class SubscriptionInfo {
   public:
    std::atomic<cr_timestamp_nsec_t> latest_delivered_time_{0};
    std::vector<CRNode*>             sub_list_;
};

}  // namespace

using channel_id_t = CRMessageBase::channel_id_t;

using subscription_map_t = std::unordered_map<channel_id_t, SubscriptionInfo, boost::hash<channel_id_t>>;

static subscription_map_t& GetSubscriptionMap() {
    static subscription_map_t subscription_map;
    return subscription_map;
};

static SubscriptionInfo* GetSubscriptionInfo(const channel_id_t channel) {
    auto& subscription_map  = GetSubscriptionMap();
    auto  subscription_find = subscription_map.find(channel);
    if (subscription_find == subscription_map.end()) {
        return nullptr;
    }
    return &subscription_find->second;
}

channel_id_t CRMessageBase::GetChannelId() const {
    return std::make_pair(GetMessageTypeIndex(), GetChannelSubId());
}

void CRMessageBase::Dispatch(const CRMessageBasePtr& message) {
    auto* subscription_info = GetSubscriptionInfo(message->GetChannelId());
    if (!subscription_info) {
        return;
    }
    for (auto& node : subscription_info->sub_list_) {
        [[maybe_unused]] const bool success = node->AddMessageToRunner(std::move(message));
        DCHECK(success);
    }
    subscription_info->latest_delivered_time_.store(GetSystemTimestampNsec());
}

bool CRMessageBase::Subscribe(const channel_id_t channel, CRNode* node) {
    auto& subscription_list = GetSubscriptionMap()[channel].sub_list_;

    if (std::find(subscription_list.begin(), subscription_list.end(), node) != subscription_list.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") "
                     << "is subscribed by the node " << node << ", skipping subscription.";
        return false;
    }
    subscription_list.emplace_back(node);
    return true;
}

void CRMessageBase::Unsubscribe(const channel_id_t channel, CRNode* node) {
    auto& subscription_map        = GetSubscriptionMap();
    auto  subscription_map_search = subscription_map.find(channel);
    if (subscription_map_search == subscription_map.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") is unknown";
        return;
    }

    if (!std::erase(subscription_map_search->second.sub_list_, node)) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") "
                     << "is not subscribed by node " << node;
    }
}

cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime(const channel_id_t channel) {
    constexpr cr_timestamp_nsec_t kDefaultDeliveredTime = 0;

    auto& subscription_map        = GetSubscriptionMap();
    auto  subscription_map_search = subscription_map.find(channel);
    if (subscription_map_search == subscription_map.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") is unknown";
        return kDefaultDeliveredTime;
    }
    return subscription_map_search->second.latest_delivered_time_.load();
}

}  // namespace cris::core
