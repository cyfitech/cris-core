#pragma once

#include <memory>
#include <string>
#include <vector>

#include "defs.h"

namespace cris::core {

class CRNodeBase;

class CRMessageBase {
   public:
    using subscription_list_t = std::vector<CRNodeBase*>;

    CRMessageBase() = default;

    CRMessageBase(const CRMessageBase&) = delete;

    CRMessageBase(CRMessageBase&&) = default;

    CRMessageBase& operator=(const CRMessageBase&) = delete;

    virtual ~CRMessageBase() = default;

    virtual const std::string GetMessageTypeName() const = 0;

    template<class message_t>
    static std::string GetMessageTypeName();

    static void Dispatch(const std::shared_ptr<CRMessageBase>& message);

   private:
    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static void Subscribe(const std::string& message_type, CRNodeBase* node);

    // Not thread-safe, do not call concurrently nor call it
    // when messages are coming
    static void Unsubscribe(const std::string& message_type, CRNodeBase* node);

    static const subscription_list_t* GetSubscriptionList(const std::string& message_type);

    friend class CRNodeBase;
};

using CRMessageBasePtr = std::shared_ptr<CRMessageBase>;

template<class message_t>
class CRMessage : public CRMessageBase {
   public:
    const std::string GetMessageTypeName() const override {
        return CRMessageBase::GetMessageTypeName<message_t>();
    }
};

template<class message_t>
std::string CRMessageBase::GetMessageTypeName() {
    static_assert(std::is_base_of_v<CRMessageBase, message_t>);
    return GetTypeName<message_t>();
}

}  // namespace cris::core
