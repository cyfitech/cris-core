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

    CRMessageBase(const CRMessageBase&) = delete;

    CRMessageBase(CRMessageBase&&) = default;

    CRMessageBase& operator=(const CRMessageBase&) = delete;

    virtual ~CRMessageBase() = default;

    virtual const std::string GetMessageTypeName() const = 0;

    template<class message_t>
    static void Subscribe(CRNodeBase* node);

    template<class message_t>
    static const subscription_list_t* GetSubscriptionList();

    template<class message_t>
    static std::string GetMessageTypeName();

    static void Dispatch(const std::shared_ptr<CRMessageBase>& message);

   private:
    static void SubscribeImpl(const std::string& message_type, CRNodeBase* node);

    static const subscription_list_t* GetSubscriptionListImpl(const std::string& message_type);
};

using CRMessageBasePtr = std::shared_ptr<CRMessageBase>;

template<class message_t>
class CRMessage {
   public:
    const std::string GetMessageTypeName() const {
        return CRMessageBase::GetMessageTypeName<message_t>();
    }
};

template<class message_t>
void CRMessageBase::Subscribe(CRNodeBase* node) {
    static_assert(std::is_base_of_v<CRMessageBase, message_t>);
    return SubscribeImpl(GetMessageTypeName<message_t>(), node);
}

template<class message_t>
const CRMessageBase::subscription_list_t* CRMessageBase::GetSubscriptionList() {
    static_assert(std::is_base_of_v<CRMessageBase, message_t>);
    return GetSubscriptionListImpl(GetMessageTypeName<message_t>());
}

template<class message_t>
std::string CRMessageBase::GetMessageTypeName() {
    static_assert(std::is_base_of_v<CRMessageBase, message_t>);
    return GetTypeName<message_t>();
}

}  // namespace cris::core
