#pragma once

#include "cris/core/defs.h"
#include "cris/core/timer/timer.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace cris::core {

class CRMessageBase;
class CRNodeBase;

template<class message_t>
concept CRMessageType = std::is_base_of_v<CRMessageBase, message_t>;

template<class callback_t, class message_t = CRMessageBase>
concept CRMessageCallbackType = std::is_base_of_v<CRMessageBase, message_t> &&
    std::is_void_v<decltype(std::declval<callback_t>()(std::declval<const std::shared_ptr<message_t>&>()))>;

class CRMessageBase {
   public:
    using channel_subid_t = std::uint64_t;
    using channel_id_t    = std::pair<std::type_index, channel_subid_t>;

    CRMessageBase() = default;

    CRMessageBase(const CRMessageBase&) = delete;

    CRMessageBase(CRMessageBase&&) = default;

    CRMessageBase& operator=(const CRMessageBase&) = delete;

    virtual ~CRMessageBase() = default;

    virtual std::string GetMessageTypeName() const = 0;

    virtual std::type_index GetMessageTypeIndex() const = 0;

    channel_subid_t GetChannelSubId() const { return sub_id_; }

    void SetChannelSubId(const channel_subid_t sub_id) { sub_id_ = sub_id; }

    channel_id_t GetChannelId() const;

    template<CRMessageType message_t>
    static cr_timestamp_nsec_t GetLatestDeliveredTime(const channel_subid_t channel_subid);

    static cr_timestamp_nsec_t GetLatestDeliveredTime(const channel_id_t channel);

    static void Dispatch(const std::shared_ptr<CRMessageBase>& message);

    constexpr static channel_subid_t kDefaultChannelSubID = 0;

   private:
    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static bool Subscribe(const channel_id_t channel, CRNodeBase* node);

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static void Unsubscribe(const channel_id_t channel, CRNodeBase* node);

    friend class CRNode;

    channel_subid_t sub_id_{kDefaultChannelSubID};
};

using CRMessageBasePtr = std::shared_ptr<CRMessageBase>;

template<class message_t>
class CRMessage : public CRMessageBase {
   public:
    std::type_index GetMessageTypeIndex() const override { return std::type_index(typeid(message_t)); }

    std::string GetMessageTypeName() const override { return GetTypeName<message_t>(); }
};

template<CRMessageType message_t>
cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime(const CRMessageBase::channel_subid_t channel_subid) {
    return GetLatestDeliveredTime(std::make_pair(std::type_index(typeid(message_t)), channel_subid));
}

}  // namespace cris::core
