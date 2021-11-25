#include "cris/core/node/named_node.h"
#include "cris/core/node/single_queue_node.h"

#include <thread>

namespace cris::core {

class CRMessageManager : public CRNamedNode<CRMessageManager, CRSingleQueueNode<>> {
   public:
    CRMessageManager(size_t queue_capacity)
        : CRNamedNode<CRMessageManager, CRSingleQueueNode<>>(queue_capacity, &CRMessageBase::Dispatch) {}

   private:
    void SubscribeHandler(std::string&& message_name, std::function<void(const CRMessageBasePtr&)>&& callback) override;
};

CRNodeBase* CRNodeBase::GetMessageManager() {
    // TODO: make them configurable
    static const size_t kMessageManagerQueueCapacity = 8192;

    static CRMessageManager manager(kMessageManagerQueueCapacity);
    return &manager;
}

void CRMessageManager::SubscribeHandler(
    std::string&&                                  message_name,
    std::function<void(const CRMessageBasePtr&)>&& callback) {
    // Message Manager never subscribe to any message
}

}  // namespace cris::core
