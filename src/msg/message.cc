#include "cris/core/msg/message.h"

#include "cris/core/msg/node.h"
#include "cris/core/utils/logging.h"

#include <boost/functional/hash.hpp>

#include <mutex>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace cris::core {

using channel_id_t = CRMessageBase::channel_id_t;

namespace {

class SubscriptionInfo {
   public:
    std::atomic<cr_timestamp_nsec_t> latest_delivered_time_{0};
    std::vector<CRNode*>             sub_list_;
};

class SubscriptionMap {
   public:
    bool Subscribe(const channel_id_t channel, CRNode* node);

    void Unsubscribe(const channel_id_t channel, CRNode* node);

    void Dispatch(const CRMessageBasePtr& message);

    cr_timestamp_nsec_t GetLatestDeliveredTime(const channel_id_t channel);

    static SubscriptionMap& GetInstance();

   private:
    std::shared_mutex                                                             mtx_;
    std::unordered_map<channel_id_t, SubscriptionInfo, boost::hash<channel_id_t>> map_;
};

SubscriptionMap& SubscriptionMap::GetInstance() {
    static SubscriptionMap subscription_map;
    return subscription_map;
};

bool SubscriptionMap::Subscribe(const channel_id_t channel, CRNode* node) {
    std::lock_guard lck(mtx_);
    auto&           subscription_list = map_[channel].sub_list_;

    if (std::find(subscription_list.begin(), subscription_list.end(), node) != subscription_list.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") "
                     << "is subscribed by the node " << node << ", skipping subscription.";
        return false;
    }
    subscription_list.emplace_back(node);
    return true;
}

void SubscriptionMap::Unsubscribe(const channel_id_t channel, CRNode* node) {
    std::lock_guard lck(mtx_);
    auto            subscription_map_search = map_.find(channel);
    if (subscription_map_search == map_.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") is unknown";
        return;
    }

    if (!std::erase(subscription_map_search->second.sub_list_, node)) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") "
                     << "is not subscribed by node " << node;
    }
}

void SubscriptionMap::Dispatch(const CRMessageBasePtr& message) {
    std::shared_lock lck(mtx_);
    auto             subscription_find = map_.find(message->GetChannelId());
    if (subscription_find == map_.end()) {
        return;
    }
    auto& subscription_info = subscription_find->second;
    for (auto& node : subscription_info.sub_list_) {
        const bool success = node->AddMessageToRunner(message);
        LOG_IF(ERROR, !success) << __func__ << ": Failed to send message to node " << node->GetName();
    }
    subscription_info.latest_delivered_time_.store(GetSystemTimestampNsec());
}

cr_timestamp_nsec_t SubscriptionMap::GetLatestDeliveredTime(const channel_id_t channel) {
    constexpr cr_timestamp_nsec_t kDefaultDeliveredTime = 0;
    std::shared_lock              lck(mtx_);

    const auto subscription_find = map_.find(channel);
    if (subscription_find == map_.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") is unknown";
        return kDefaultDeliveredTime;
    }
    return subscription_find->second.latest_delivered_time_.load();
}

}  // namespace

channel_id_t CRMessageBase::GetChannelId() const {
    return std::make_pair(GetMessageTypeIndex(), GetChannelSubId());
}

void CRMessageBase::Dispatch(const CRMessageBasePtr& message) {
    return SubscriptionMap::GetInstance().Dispatch(message);
}

bool CRMessageBase::Subscribe(const channel_id_t channel, CRNode* node) {
    return SubscriptionMap::GetInstance().Subscribe(channel, node);
}

void CRMessageBase::Unsubscribe(const channel_id_t channel, CRNode* node) {
    return SubscriptionMap::GetInstance().Unsubscribe(channel, node);
}

cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime(const channel_id_t channel) {
    return SubscriptionMap::GetInstance().GetLatestDeliveredTime(channel);
}

}  // namespace cris::core
