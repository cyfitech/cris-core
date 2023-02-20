#pragma once

#include "cris/core/utils/defs.h"
#include "cris/core/utils/time.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace cris::core {

class CRMessageBase;
class CRNode;

template<class message_t>
concept CRMessageType = std::is_base_of_v<CRMessageBase, message_t>;

class CRMessageBase {
   public:
    using channel_subid_t = std::uint64_t;
    using channel_id_t    = std::pair<std::type_index, channel_subid_t>;

    CRMessageBase() = default;

    CRMessageBase(const CRMessageBase&) = delete;

    CRMessageBase(CRMessageBase&&) = default;

    CRMessageBase& operator=(const CRMessageBase&) = delete;

    CRMessageBase& operator=(CRMessageBase&&) = default;

    virtual ~CRMessageBase() = default;

    virtual std::string GetMessageTypeName() const = 0;

    virtual std::type_index GetMessageTypeIndex() const = 0;

    channel_subid_t GetChannelSubId() const { return sub_id_; }

    channel_id_t GetChannelId() const;

    template<CRMessageType message_t>
    static cr_timestamp_nsec_t GetLatestDeliveredTime(const channel_subid_t channel_subid);

    static cr_timestamp_nsec_t GetLatestDeliveredTime(const channel_id_t channel);

    constexpr static channel_subid_t kDefaultChannelSubID = 0;

   private:
    void SetChannelSubId(const channel_subid_t sub_id) { sub_id_ = sub_id; }

    static std::shared_lock<std::shared_mutex> SubscriptionReadLock();

    static std::unique_lock<std::shared_mutex> SubscriptionWriteLock();

    static void Dispatch(const std::shared_ptr<CRMessageBase>& message);

    // Must be called with SubscriptionWriteLock.
    static bool SubscribeUnsafe(
        const channel_id_t                         channel,
        CRNode*                                    node,
        const std::unique_lock<std::shared_mutex>& lck);

    // Must be called with SubscriptionWriteLock.
    static void UnsubscribeUnsafe(
        const channel_id_t                         channel,
        CRNode*                                    node,
        const std::unique_lock<std::shared_mutex>& lck);

    static bool Subscribe(const channel_id_t channel, CRNode* node);

    static void Unsubscribe(const channel_id_t channel, CRNode* node);

    friend class CRNode;

    channel_subid_t sub_id_{kDefaultChannelSubID};
};

using CRMessageBasePtr = std::shared_ptr<CRMessageBase>;

template<class message_t>
class CRMessage : public CRMessageBase {
   public:
    std::type_index GetMessageTypeIndex() const override { return static_cast<std::type_index>(typeid(message_t)); }

    std::string GetMessageTypeName() const override { return GetTypeName<message_t>(); }
};

template<CRMessageType message_t>
cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime(const CRMessageBase::channel_subid_t channel_subid) {
    return GetLatestDeliveredTime(std::make_pair(static_cast<std::type_index>(typeid(message_t)), channel_subid));
}

}  // namespace cris::core
