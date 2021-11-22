#pragma once

#include "cris/core/defs.h"
#include "cris/core/timer/timer.h"

#include <memory>
#include <string>
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
    CRMessageBase() = default;

    CRMessageBase(const CRMessageBase&) = delete;

    CRMessageBase(CRMessageBase&&) = default;

    CRMessageBase& operator=(const CRMessageBase&) = delete;

    virtual ~CRMessageBase() = default;

    virtual const std::string GetMessageTypeName() const = 0;

    template<CRMessageType message_t>
    static std::string GetMessageTypeName();

    template<CRMessageType message_t>
    static cr_timestamp_nsec_t GetLatestDeliveredTime();

    static void Dispatch(const std::shared_ptr<CRMessageBase>& message);

   private:
    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static void Subscribe(const std::string& message_type, CRNodeBase* node);

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static void Unsubscribe(const std::string& message_type, CRNodeBase* node);

    static cr_timestamp_nsec_t GetLatestDeliveredTimeImpl(const std::string& message_type);

    class SubscriptionInfo {
       public:
        std::atomic<cr_timestamp_nsec_t> latest_delivered_time_{0};
        std::vector<CRNodeBase*>         sub_list_;
    };

    static std::map<std::string, SubscriptionInfo>* GetSubscriptionMap();

    static SubscriptionInfo* GetSubscriptionInfo(const std::string& message_type);

    friend class CRNode;
};

using CRMessageBasePtr = std::shared_ptr<CRMessageBase>;

template<class message_t>
class CRMessage : public CRMessageBase {
   public:
    const std::string GetMessageTypeName() const override { return CRMessageBase::GetMessageTypeName<message_t>(); }
};

template<CRMessageType message_t>
std::string CRMessageBase::GetMessageTypeName() {
    return GetTypeName<message_t>();
}

template<CRMessageType message_t>
cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime() {
    return GetLatestDeliveredTimeImpl(GetMessageTypeName<message_t>());
}

}  // namespace cris::core
