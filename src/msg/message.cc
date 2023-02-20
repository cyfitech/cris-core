#include "cris/core/msg/message.h"

#include "cris/core/msg/node.h"
#include "cris/core/utils/logging.h"

#include <boost/functional/hash.hpp>

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
    std::unordered_map<channel_id_t, SubscriptionInfo, boost::hash<channel_id_t>> map_;
};

SubscriptionMap& SubscriptionMap::GetInstance() {
    static SubscriptionMap subscription_map;
    return subscription_map;
};

bool SubscriptionMap::Subscribe(const channel_id_t channel, CRNode* node) {
    auto& subscription_list = map_[channel].sub_list_;

    if (std::find(subscription_list.begin(), subscription_list.end(), node) != subscription_list.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") "
                     << "is subscribed by the node " << node << ", skipping subscription.";
        return false;
    }
    subscription_list.emplace_back(node);
    return true;
}

void SubscriptionMap::Unsubscribe(const channel_id_t channel, CRNode* node) {
    auto subscription_map_search = map_.find(channel);
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
    auto subscription_find = map_.find(message->GetChannelId());
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

    const auto subscription_find = map_.find(channel);
    if (subscription_find == map_.end()) {
        LOG(WARNING) << __func__ << ": Channel (" << channel.first.name() << ", " << channel.second << ") is unknown";
        return kDefaultDeliveredTime;
    }
    return subscription_find->second.latest_delivered_time_.load();
}

}  // namespace

static std::shared_mutex& SubscriptionMutex() {
    static std::shared_mutex mtx;
    return mtx;
}

channel_id_t CRMessageBase::GetChannelId() const {
    return std::make_pair(GetMessageTypeIndex(), GetChannelSubId());
}

void CRMessageBase::Dispatch(const CRMessageBasePtr& message) {
    auto lck = SubscriptionReadLock();
    return SubscriptionMap::GetInstance().Dispatch(message);
}

std::shared_lock<std::shared_mutex> CRMessageBase::SubscriptionReadLock() {
    return std::shared_lock(SubscriptionMutex());
}

std::unique_lock<std::shared_mutex> CRMessageBase::SubscriptionWriteLock() {
    return std::unique_lock(SubscriptionMutex());
}

bool CRMessageBase::SubscribeUnsafe(
    const channel_id_t                         channel,
    CRNode*                                    node,
    const std::unique_lock<std::shared_mutex>& lck) {
    if (!lck.owns_lock()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Must be called with owning lock.";
        return false;
    }
    return SubscriptionMap::GetInstance().Subscribe(channel, node);
}

void CRMessageBase::UnsubscribeUnsafe(
    const channel_id_t                         channel,
    CRNode*                                    node,
    const std::unique_lock<std::shared_mutex>& lck) {
    if (!lck.owns_lock()) [[unlikely]] {
        LOG(ERROR) << __func__ << ": Must be called with owning lock.";
        return;
    }
    return SubscriptionMap::GetInstance().Unsubscribe(channel, node);
}

bool CRMessageBase::Subscribe(const channel_id_t channel, CRNode* node) {
    return SubscribeUnsafe(channel, node, SubscriptionWriteLock());
}

void CRMessageBase::Unsubscribe(const channel_id_t channel, CRNode* node) {
    return UnsubscribeUnsafe(channel, node, SubscriptionWriteLock());
}

cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime(const channel_id_t channel) {
    auto lck = SubscriptionReadLock();
    return SubscriptionMap::GetInstance().GetLatestDeliveredTime(channel);
}

}  // namespace cris::core
