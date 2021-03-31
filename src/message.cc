#include <map>

#include "message/base.h"
#include "node/base.h"

namespace cris::core {

namespace impl {

std::map<std::string, CRMessageBase::subscription_list_t> subscription_map{};

}  // namespace impl

void CRMessageBase::Dispatch(const CRMessageBasePtr& message) {
    auto* subscription_list = GetSubscriptionListImpl(message->GetMessageTypeName());
    if (!subscription_list) {
        return;
    }
    for (auto&& node : *subscription_list) {
        auto* queue = node->MessageQueueMapper(message);
        if (!queue) {
            // TODO WARNING
            continue;
        }
        queue->AddMessage(CRMessageBasePtr(message));
        node->Kick();
    }
}

void CRMessageBase::SubscribeImpl(const std::string& message_type, CRNodeBase* node) {
    impl::subscription_map[message_type].emplace_back(node);
}

const CRMessageBase::subscription_list_t* CRMessageBase::GetSubscriptionListImpl(
    const std::string& message_type) {
    auto subscription_find = impl::subscription_map.find(message_type);
    if (subscription_find == impl::subscription_map.end()) {
        return nullptr;
    }
    return &subscription_find->second;
}

}  // namespace cris::core
