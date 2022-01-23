#pragma once

#include "cris/core/defs.h"
#include "cris/core/timer/timer.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
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
    using channel_id_t = std::uint64_t;

    CRMessageBase() = default;

    CRMessageBase(const CRMessageBase&) = delete;

    CRMessageBase(CRMessageBase&&) = default;

    CRMessageBase& operator=(const CRMessageBase&) = delete;

    virtual ~CRMessageBase() = default;

    virtual const std::string GetMessageTypeName() const = 0;

    virtual const std::type_index GetMessageTypeIndex() const = 0;

    template<CRMessageType message_t>
    static cr_timestamp_nsec_t GetLatestDeliveredTime();

    static cr_timestamp_nsec_t GetLatestDeliveredTime(const std::type_index message_type);

    static void Dispatch(const std::shared_ptr<CRMessageBase>& message);

   private:
    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static bool Subscribe(const std::type_index message_type, CRNodeBase* node);

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static void Unsubscribe(const std::type_index message_type, CRNodeBase* node);

    friend class CRNode;
};

using CRMessageBasePtr = std::shared_ptr<CRMessageBase>;

template<class message_t>
class CRMessage : public CRMessageBase {
   public:
    const std::type_index GetMessageTypeIndex() const override { return std::type_index(typeid(message_t)); }

    const std::string GetMessageTypeName() const override { return GetTypeName<message_t>(); }
};

template<CRMessageType message_t>
cr_timestamp_nsec_t CRMessageBase::GetLatestDeliveredTime() {
    return GetLatestDeliveredTime(std::type_index(typeid(message_t)));
}

}  // namespace cris::core
